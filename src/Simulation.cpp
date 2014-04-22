
#include "Precomp_OpenGL.h"
#include "Simulation.hpp"
#include "Resources.hpp"
#include "ParamUtils.hpp"
#include "OGL_Utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

unsigned int _NKEYS = 0;

#define DivCeil(num, divider) (((num) + (divider) - 1) / (divider)) 
#define IntCeil(num, divider) ((((num) + (divider) - 1) / (divider)) * (divider))

#define  M_PI   3.14159265358979323846	/* pi */

Simulation::Simulation() :
      mCells(NULL),
      bDumpParticlesData(false),
      mPositions(NULL),
      mVelocities(NULL),
      mPredictions(NULL),
      mDeltas(NULL),
      mFriendsList(NULL)
{
}

Simulation::~Simulation()
{
    glFinish();

    // Delete buffers
    delete[] mCells;
    delete[] mPositions;
    delete[] mVelocities;
}

void Simulation::CreateParticles()
{
    // Reset velocities
    memset(mVelocities, 0, sizeof(mVelocities[0]) * Params.particleCount);

    // Choose between 2D and 3D
    float dim = 3.0;

    // Compute particle count per axis
    int ParticlesPerAxis = (int)ceil(pow(Params.particleCount, 1 / dim));

    // Build particles blcok
    float d = Params.h * Params.setupSpacing;
    float offsetX = (1.0f - ParticlesPerAxis * d) / 2.0f;
    float offsetY = 0.3f;
    float offsetZ = (1.0f - ParticlesPerAxis * d) / 2.0f;
    for (uint i = 0; i < Params.particleCount; i++)
    {
        uint x = ((uint)(i / pow(ParticlesPerAxis, 1.0f)) % ParticlesPerAxis);
        uint y = ((uint)(i / pow(ParticlesPerAxis, 0.0f)) % ParticlesPerAxis);
        uint z = ((uint)(i / pow(ParticlesPerAxis, 2.0f)) % ParticlesPerAxis)  * (dim == 3);

        mPositions[i][0] = offsetX + x * d;
        mPositions[i][1] = offsetY + y * d;
        mPositions[i][2] = offsetZ + z * d;
        mPositions[i][3] = 0;
    }

    mNumGroups = DivCeil(Params.particleCount,Params.localSize);
}

void DumpFloatArrayCompare(char* szFilename, char* szTitle1, char* szTitle2, float* arr1, float* arr2, int itemsCount)
{
    ofstream fdmp(szFilename);
    fdmp << szTitle1 << "," << szTitle2 << ",diff" << endl;
    for (int i = 0; i < itemsCount; i++) 
        fdmp << arr1[i] << "," << arr2[i] << "," << arr2[i] - arr1[i] << endl;
}

const string *Simulation::ShaderFileList()
{
    static const string shaders[] =
    {
        "predict_positions.cms",
        "compute_keys.cms",
        "histogram.cms",
        "scanhistograms1.cms",
        "scanhistograms2.cms",
        "pastehistograms.cms",
        "reorder.cms",
        "sort_particles.cms",
        "update_cells.cms",
        "build_friends_list.cms",
        "reset_grid.cms",
        "compute_scaling.cms",
        "compute_delta.cms",
        "update_predicted.cms",
        "pack_data.cms",
        "update_velocities.cms",
        "apply_viscosity.cms",
        "apply_vorticity.cms",
        ""
    };

    return shaders;
}

// Define bind points
#define BP_UBO_PARAMETERS 10

GLuint GenBuffer(GLenum target, int size, void* pData)
{
    GLuint ret;
    glGenBuffers(1, &ret);
    glBindBuffer(target, ret);
    glBufferData(target, size, pData, GL_DYNAMIC_COPY);
    glBindBuffer(target, 0);
    return ret;
}

GLuint GenTextureBuffer(GLenum format, GLuint bufferId)
{
    GLuint ret;
    glGenTextures(1, &ret);
    glBindTexture(GL_TEXTURE_BUFFER, ret);
    glTexBuffer(GL_TEXTURE_BUFFER, format, bufferId);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
    return ret;
}

bool Simulation::InitShaders()
{
    // Build shader header
    string header = "#version 430\n#define GLSL_COMPILER\n";

    // Add Parameters 
    header += "#line 1\n" + getKernelSource("parameters.hpp") + "\n";

    // Add defines
    std::ostringstream defines;
    defines << "#define LOCAL_SIZE             " << (int)Params.localSize                      << ""   << endl; // no brackets!
    defines << "#define END_OF_CELL_LIST      (" << (int)(-1)                                  << ")"  << endl;
    defines << "#define MAX_PARTICLES_COUNT   (" << (int)(Params.particleCount)                << ")"  << endl;
    defines << "#define MAX_FRIENDS_CIRCLES   (" << (int)(Params.friendsCircles)               << ")"  << endl;
    defines << "#define MAX_FRIENDS_IN_CIRCLE (" << (int)(Params.particlesPerCircle)           << ")"  << endl;
    defines << "#define FRIENDS_BLOCK_SIZE    (" << (int)(Params.particleCount * Params.friendsCircles) << ")" << endl;
    defines << "#define GRID_BUF_SIZE         (" << (int)(Params.gridBufSize)                  << ")"  << endl;
    defines << "#define POLY6_FACTOR          (" << 315.0f / (64.0f * M_PI * pow(Params.h, 9)) << "f)" << endl;
    defines << "#define GRAD_SPIKY_FACTOR     (" << 45.0f / (M_PI * pow(Params.h, 6))          << "f)" << endl;
    header += "#line 1\n" + defines.str();

    // Add Utils
    header += "#line 1\n" + getKernelSource("utils.cms") + "\n";

    // Load and compile all shaders
    const string *pShaders = ShaderFileList();
    for (int iSrc = 0; pShaders[iSrc] != ""; iSrc++)
    {
        // Get shader name (no ext)
        size_t lastdot = pShaders[iSrc].find_last_of(".");
        string name = pShaders[iSrc].substr(0, lastdot);

        // Get source
        string src = header + "#line 1\n" + getKernelSource(pShaders[iSrc]);

        // compile shader
        if (!(mPrograms[name] = OGLU_LoadProgram(name, src, GL_COMPUTE_SHADER)))
            return false;
    }

    // Copy Parameters to GPU (parameters buffer object)
    mGLParametersUBO = GenBuffer(GL_UNIFORM_BUFFER, sizeof(Params), &Params);
    glBindBufferRange(GL_UNIFORM_BUFFER, BP_UBO_PARAMETERS, mGLParametersUBO, 0, sizeof(Params));

    return true;
}

void Simulation::InitBuffers()
{
    // Define CL buffer sizes
    mBufferSizeParticles      = Params.particleCount * sizeof(vec4);
    mBufferSizeParticlesList  = Params.particleCount * sizeof(int);

    // Allocate CPU buffers
    delete[] mPositions;   mPositions   = new vec4[Params.particleCount];
    delete[] mVelocities;  mVelocities  = new vec4[Params.particleCount];
    delete[] mPredictions; mPredictions = new vec4[Params.particleCount]; // (used for debugging)
    delete[] mDeltas;      mDeltas      = new vec4[Params.particleCount]; // (used for debugging)
    delete[] mFriendsList; mFriendsList = new uint[Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle)]; // (used for debugging)

    // Position particles
    CreateParticles();

    mPredictedPingSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, &mPositions[0]);
    mPredictedPingTBO = GenTextureBuffer(GL_RGBA32F, mPredictedPingSBO);
    mPredictedPongSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mPredictedPongTBO = GenTextureBuffer(GL_RGBA32F, mPredictedPongSBO);
    mDeltaSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mDeltaTBO = GenTextureBuffer(GL_RGBA32F, mDeltaSBO);
    mOmegasSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mOmegasTBO = GenTextureBuffer(GL_RGBA32F, mOmegasSBO);
    mDensitySBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(float) * Params.particleCount, NULL);
    mDensityTBO = GenTextureBuffer(GL_R32F, mDensitySBO);

    // Radix buffers
    _NKEYS = IntCeil(Params.particleCount, _ITEMS * _GROUPS);
    mInKeysSBO         = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mInKeysTBO         = GenTextureBuffer(GL_R32UI, mInKeysSBO);
    mInPermutationSBO  = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mInPermutationTBO  = GenTextureBuffer(GL_R32UI, mInPermutationSBO);
    mOutKeysSBO        = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mOutKeysTBO        = GenTextureBuffer(GL_R32UI, mOutKeysSBO);
    mOutPermutationSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _NKEYS, NULL);
    mOutPermutationTBO = GenTextureBuffer(GL_R32UI, mOutPermutationSBO);    
    mHistogramSBO      = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _RADIX * _GROUPS * _ITEMS, NULL);
    mHistogramTBO      = GenTextureBuffer(GL_R32UI, mHistogramSBO);
    mGlobSumSBO        = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _HISTOSPLIT, NULL);
    mGlobSumTBO        = GenTextureBuffer(GL_R32UI, mGlobSumSBO);
    mHistoTempSBO      = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(uint) * _HISTOSPLIT, NULL);
    mHistoTempTBO      = GenTextureBuffer(GL_R32UI, mHistoTempSBO);

    // Copy mPositions (Host) => mPositionsPingBuffer (GPU) 
    mPositionsPingSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, &mPositions[0]);
    mPositionsPingTBO = GenTextureBuffer(GL_RGBA32F, mPositionsPingSBO);
    mPositionsPongSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, NULL);
    mPositionsPongTBO = GenTextureBuffer(GL_RGBA32F, mPositionsPongSBO);

    // Copy mVelocities (Host) => mVelocitiesBuffer (GPU)
    mVelocitiesSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, sizeof(vec4) * Params.particleCount, &mVelocities[0]);
    mVelocitiesTBO = GenTextureBuffer(GL_RGBA32F, mVelocitiesSBO);
}

void Simulation::InitCells()
{
    // Allocate host buffers
    delete[] mCells;
    mCells = new int[Params.gridBufSize * 2];
    for (uint i = 0; i < Params.gridBufSize * 2; ++i)
        mCells[i] = END_OF_CELL_LIST;

    // Write buffer for cells
    mCellsSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, Params.gridBufSize * 2 * sizeof(int), mCells);
    mCellsTBO = GenTextureBuffer(GL_R32I, mCellsSBO);

    // Init Friends list buffer
    int BufSize = Params.particleCount * Params.friendsCircles * (1 + Params.particlesPerCircle) * sizeof(int);
    memset(mFriendsList, 0, BufSize);

    mFriendsListSBO = GenBuffer(GL_SHADER_STORAGE_BUFFER, BufSize, mFriendsList);
    mFriendsListTBO = GenTextureBuffer(GL_R32UI, mFriendsListSBO);
}

void Simulation::SetupComputeShader(char* szPrgramName)
{
    OGLU_StartTimingSection(szPrgramName);
    glUseProgram(g_SelectedProgram = mPrograms[szPrgramName]);
}

void Simulation::DispatchComputeShader(int WorkItemCount, int LocalSizeX, int LocalSizeY)
{
    // Compute groups count
    int groups = DivCeil(Params.particleCount, LocalSizeX * LocalSizeY);

    // Execute shader
    glDispatchCompute(groups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

int dumpSession = 0;
int dumpCounter = 0;
int cycleCounter = 0;

void Simulation::updateVelocities()
{
    // Setup shader
    SetupComputeShader("update_velocities");
    glBindImageTexture(0, mPositionsPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(2, mVelocitiesTBO,    0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
}

void Simulation::applyViscosity()
{
    // Setup shader
    OGLU_StartTimingSection("apply_viscosity");
    glUseProgram(g_SelectedProgram = mPrograms["apply_viscosity"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, mFriendsListTBO,   0, GL_FALSE, 0, GL_READ_ONLY, GL_R32I);
    glBindImageTexture(2, mVelocitiesTBO,    0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(3, mOmegasTBO,        0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(0/*N*/,        Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::applyVorticity()
{return;
    // Setup shader
    OGLU_StartTimingSection("apply_vorticity");
    glUseProgram(g_SelectedProgram = mPrograms["apply_vorticity"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mVelocitiesTBO,    0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(2, mOmegasTBO,        0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(3, mFriendsListTBO,   0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::predictPositions()
{
    // Setup shader
    OGLU_StartTimingSection("predict_positions");
    glUseProgram(g_SelectedProgram = mPrograms["predict_positions"]);
    glBindImageTexture(0, mPositionsPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mPredictedPingTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(2, mVelocitiesTBO,    0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glUniform1i(0/*N*/,        Params.particleCount);
    glUniform1i(1/*pauseSim*/, bPauseSim);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void DumpFriendsList(uint* friendList, float* position, char* szFilename)
{
    ofstream fdmp(szFilename);

    // Compute histogram data
    int FriendsCountBlockSize = Params.particleCount * Params.friendsCircles;
    for (unsigned int iPart = 0; iPart < Params.particleCount; iPart++)
    {
        fdmp << "Particle #" << iPart << ":" << endl;
        for (unsigned int iCircle = 0; iCircle < Params.friendsCircles; iCircle++)
        {
            uint circleFriends = friendList[iCircle * Params.particleCount + iPart];
            float px = position[iPart * 4 + 0];
            float py = position[iPart * 4 + 1];
            float pz = position[iPart * 4 + 2];

            // Write circle header
            fdmp << "  C" << iCircle << " [" << circleFriends << "]: ";

            // Make sure circleFriends is not bigger than Params.particlesPerCircle
            if (circleFriends > Params.particlesPerCircle)
                circleFriends = Params.particlesPerCircle;

            // Print friends IDs
            for (uint iFriend = 0; iFriend < circleFriends; iFriend++)
            {
                int friendIndex = friendList[FriendsCountBlockSize + 
                                             iCircle * (Params.particleCount * Params.particlesPerCircle) +  
                                             iFriend * (Params.particleCount) + 
                                             iPart];
                float dx = position[friendIndex * 4 + 0] - px;
                float dy = position[friendIndex * 4 + 1] - py;
                float dz = position[friendIndex * 4 + 2] - pz;
                fdmp << friendIndex << "(" << sqrt(dx*dx+dy*dy+dz*dz) / Params.h << ") ";
            }

            fdmp << endl;
        }
        fdmp << endl;
    }
}

void Simulation::buildFriendsList()
{
    // Setup shader
    OGLU_StartTimingSection("build_friends_list");
    glUseProgram(g_SelectedProgram = mPrograms["build_friends_list"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mCellsTBO,         0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glBindImageTexture(2, mFriendsListTBO,   0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUniform1i(0/*N*/,        Params.particleCount);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_BUFFER, mPredictedPingTBO);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_BUFFER, mCellsTBO);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Setup shader
    OGLU_StartTimingSection("reset_grid");
    glUseProgram(g_SelectedProgram = mPrograms["reset_grid"]);
    glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32I);
    glBindImageTexture(1, mCellsTBO,  0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUniform1i(0/*N*/,        Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
}

void Simulation::updatePredicted(int iterationIndex)
{
    OGLU_StartTimingSection("%d_update_predicted", iterationIndex);
    glUseProgram(g_SelectedProgram = mPrograms["update_predicted"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, mDeltaTBO,         0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::packData()
{
    OGLU_StartTimingSection("pack_data");
    glUseProgram(g_SelectedProgram = mPrograms["pack_data"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);
    glBindImageTexture(1, mDensityTBO,       0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32F);
    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::computeDelta(int iterationIndex)
{
    OGLU_StartTimingSection("%d_compute_delta", iterationIndex);
    glUseProgram(g_SelectedProgram = mPrograms["compute_delta"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mDeltaTBO,         0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(2, mFriendsListTBO,   0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glUniform1i(0/*N*/, Params.particleCount);
    glUniform1f(1/*wave_generator*/, fWavePos);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_BUFFER, mPredictedPingTBO);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::computeScaling(int iterationIndex)
{
    OGLU_StartTimingSection("%d_compute_scaling", iterationIndex);
    glUseProgram(g_SelectedProgram = mPrograms["compute_scaling"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_RGBA32F);
    glBindImageTexture(1, mDensityTBO,       0, GL_FALSE, 0, GL_WRITE_ONLY,  GL_R32F);
    glBindImageTexture(2, mFriendsListTBO,   0, GL_FALSE, 0, GL_READ_ONLY,   GL_R32I);
    
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_BUFFER, mPredictedPingTBO);

    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::updateCells()
{
    OGLU_StartTimingSection("update_cells");
    glUseProgram(g_SelectedProgram = mPrograms["update_cells"]);
    glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32I);
    glBindImageTexture(1, mCellsTBO,  0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32I);
    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);
    //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void Simulation::radixsort()
{
    // Setup
    OGLU_StartTimingSection("radixsort");
    glUseProgram(g_SelectedProgram = mPrograms["compute_keys"]);
    glBindImageTexture(0, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_RGBA32F);
    glBindImageTexture(1, mInKeysTBO,        0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    glBindImageTexture(2, mInPermutationTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);

    glUniform1i(0/*N*/, Params.particleCount);

    // Execute shader
    glDispatchCompute(DivCeil(_NKEYS,Params.localSize), 1, 1);

    for (size_t pass = 0; pass < _PASS; pass++)
    {
        // Histogram(pass);
        const size_t h_nblocitems = _ITEMS;
        const size_t h_nbitems = _GROUPS * _ITEMS;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["histogram"]);
        glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glBindImageTexture(1, mHistogramTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        glUniform1i(0, pass);
        glUniform1i(1, _NKEYS);

        // Execute shader
        glDispatchCompute(h_nbitems / h_nblocitems, 1, 1);

        const size_t sh1_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t sh1_nblocitems = sh1_nbitems / _HISTOSPLIT ;
        const int maxmemcache = glm::max(_HISTOSPLIT, _ITEMS * _GROUPS * _RADIX / _HISTOSPLIT);

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["scanhistograms1"]);
        glBindImageTexture(0, mHistogramTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_R32UI);
        glBindImageTexture(1, mGlobSumTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        // Execute shader
        glDispatchCompute(sh1_nbitems / sh1_nblocitems, 1, 1);

        const size_t sh2_nbitems = _HISTOSPLIT / 2;
        const size_t sh2_nblocitems = sh2_nbitems;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["scanhistograms2"]);
        glBindImageTexture(0, mGlobSumTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_R32UI);
        glBindImageTexture(1, mHistoTempTBO, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

        // Execute shader
        glDispatchCompute(sh2_nbitems / sh2_nblocitems, 1, 1);

        // The diff here for i>0 should be okay because the rest of the data is never written to
        // TODO: Also why is that even needed at all? Seems like now that we need a second shader anyway
        // because of the compile-time local_size it might be better just to write a different shader
        // which gets rid of this temp buffer completely
        ///*!*/CompareIntBuffers(mQueue, mHistoTempBuffer, mHistoTempSBO);

        const size_t ph_nbitems = _RADIX * _GROUPS * _ITEMS / 2;
        const size_t ph_nblocitems = ph_nbitems / _HISTOSPLIT;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["pastehistograms"]);
        glBindImageTexture(0, mHistogramTBO, 0, GL_FALSE, 0, GL_READ_WRITE,  GL_R32UI);
        glBindImageTexture(1, mGlobSumTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);

        // Execute shader
        glDispatchCompute(ph_nbitems / ph_nblocitems, 1, 1);

        // Reorder(pass);
        const size_t r_nblocitems = _ITEMS;
        const size_t r_nbitems = _GROUPS * _ITEMS;

        // Setup
        glUseProgram(g_SelectedProgram = mPrograms["reorder"]);
        glBindImageTexture(0, mInKeysTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glBindImageTexture(1, mOutKeysTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        glBindImageTexture(2, mHistogramTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glBindImageTexture(3, mInPermutationTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
        glBindImageTexture(4, mOutPermutationTBO, 0, GL_FALSE, 0, GL_READ_ONLY,  GL_R32UI);
        glUniform1i(0, pass);
        glUniform1i(1, _NKEYS);

        // Execute shader
        glDispatchCompute(r_nbitems / r_nblocitems, 1, 1);

        GLuint tmpGL;

        tmpGL = mInKeysSBO;
        mInKeysSBO = mOutKeysSBO;
        mOutKeysSBO = tmpGL;

        tmpGL = mInKeysTBO;
        mInKeysTBO = mOutKeysTBO;
        mOutKeysTBO = tmpGL;

        tmpGL = mInPermutationSBO;
        mInPermutationSBO = mOutPermutationSBO;
        mOutPermutationSBO = tmpGL;
        tmpGL = mInPermutationTBO;
        mInPermutationTBO = mOutPermutationTBO;
        mOutPermutationTBO = tmpGL;
    }

    // Setup shader
    glUseProgram(g_SelectedProgram = mPrograms["sort_particles"]);
    glBindImageTexture(0, mInPermutationTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
    glBindImageTexture(1, mPositionsPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, mPositionsPongTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY,  GL_RGBA32F);
    glBindImageTexture(3, mPredictedPingTBO, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(4, mPredictedPongTBO, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glUniform1i(0/*N*/,        Params.particleCount);

    // Execute shader
    glDispatchCompute(mNumGroups, 1, 1);

    // Double buffering of positions and velocity buffers
    GLuint tmpGL;
    
    tmpGL = mPositionsPingSBO;
    mPositionsPingSBO = mPositionsPongSBO;
    mPositionsPongSBO = tmpGL;
    tmpGL = mPositionsPingTBO;
    mPositionsPingTBO = mPositionsPongTBO;
    mPositionsPongTBO = tmpGL;

    tmpGL = mPredictedPingSBO;
    mPredictedPingSBO = mPredictedPongSBO;
    mPredictedPongSBO = tmpGL;
    tmpGL = mPredictedPingTBO;
    mPredictedPingTBO = mPredictedPongTBO;
    mPredictedPongTBO = tmpGL;
}

void Simulation::Step()
{
    cycleCounter++;

    // Why is this here?
    glFinish();

    // Predicit positions
    this->predictPositions();

    // sort particles buffer
    if (!bPauseSim)
        this->radixsort();

    // Update cells
    this->updateCells();

    // Build friends list
    this->buildFriendsList();

    for (unsigned int i = 0; i < Params.simIterations; ++i)
    {
        // Compute scaling value
        this->computeScaling(i);

        // Compute position delta
        this->computeDelta(i);

        // Update predicted position
        this->updatePredicted(i);
    }

    // Place density in "mPredictedPingBuffer[x].w"
    this->packData();

    // Recompute velocities
    this->updateVelocities();

    // Update vorticity and Viscosity
    this->applyViscosity();
    this->applyVorticity();

    // [DEBUG] Read back friends information (if needed)
    if (bReadFriendsList || bDumpParticlesData)
    {
        CopyBufferToHost buf(GL_SHADER_STORAGE_BUFFER, mPositionsPingSBO);
        memcpy(mFriendsList, buf.pBytes, buf.size);
    }

    // [DEBUG] Do we need to dump particle data
    if (bDumpParticlesData)
    {
        // Turn off flag
        bDumpParticlesData = false;

        // Read data
        CopyBufferToHost buf(GL_SHADER_STORAGE_BUFFER, mPositionsPingSBO);

        // Save to disk
        ofstream f("particles_pos.bin", ios::out | ios::trunc | ios::binary);
        f.seekp(0);
        f.write((const char *)buf.pBytes, buf.size);
        f.close();
    }
}
