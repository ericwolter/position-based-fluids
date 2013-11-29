#include "Runner.hpp"
#include "ParamUtils.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef _WINDOWS
#include <io.h>
#endif

#include <GLFW/glfw3.h>

#include <fstream>
using std::ifstream;

#if defined(MAKE_VIDEO)
#include <unistd.h>
static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;
#endif // MAKE_VIDEO

bool Runner::DetectResourceChanges() 
{
    // Assume nothing change
    bool bChanged = false;

    // Scan all file for change
    list<pair<string, time_t> >::iterator iter;
    for(iter = mFilesTrack.begin(); iter != mFilesTrack.end(); iter++)
    {
        // Get file stat
        struct stat fileStat;
        stat(iter->first.c_str(), &fileStat);

        // Compare for change with stored stat
        if (iter->second != fileStat.st_mtime)
        {
            bChanged = true;
            break;
        }
    }

    // Exit if not change was detected
    if (!bChanged)
        return false;

    // Test exclusive lock code
    // ifstream ifs = ifstream(mFilesTrack.begin()->first.c_str());

    // change was detected: make sure that all files are openable (no one is holding the files open)
    for(iter = mFilesTrack.begin(); iter != mFilesTrack.end(); iter++)
    {
        // Check if file can be open exclusivly
        bool ExclusiveOpen;
        #ifdef _WINDOWS
            int fd;
            int openResult = _sopen_s(&fd, iter->first.c_str(), O_RDWR, _SH_DENYRW, 0);
            if (openResult == 0) _close(fd);
            ExclusiveOpen = openResult == 0;
        #else
            int openResult = open (iter->first.c_str(), O_RDWR);
            int lockResult = openResult < 0 ? -1 : flock (openResult, LOCK_EX | LOCK_NB);
            ExclusiveOpen = lockResult == 0;
            if (lockResult == 0) flock (openResult, LOCK_UN);
            if (openResult > 0) close(openResult);
        #endif

        // If failed to open file exclusivly, exit (we will try again later)
        if (!ExclusiveOpen)
            return false;
    }

    // There was a change AND all files are openable (in exclusive mode, ie: no one else accessing them) we can report the change back
    for(iter = mFilesTrack.begin(); iter != mFilesTrack.end(); iter++)
    {
        // Get file stat
        struct stat fileStat;
        stat(iter->first.c_str(), &fileStat);
        iter->second = fileStat.st_mtime;
    }

    return true;
}

void Runner::run(Simulation& simulation, CVisual& renderer)
{
    // Create resource tracking file list (Kernels)
    time_t defaultTime = 0;
    const string* pKernels = simulation.KernelFileList();
    for (int iSrc = 0; pKernels[iSrc] != ""; iSrc++)
        mFilesTrack.push_back(make_pair(getPathForKernel(pKernels[iSrc]), defaultTime));

    // Append parameter file to resource tracking file list
    mFilesTrack.push_back(make_pair(getPathForScenario("dam_coarse.par"), defaultTime));

    // Init render (background, camera etc...)
    renderer.initSystemVisual(simulation);
    renderer.initParticlesVisual(SORTING);

    #if defined(MAKE_VIDEO)
        const string cmd = "ffmpeg -r 30 -f rawvideo -pix_fmt rgb24 "
                           "-s 1280x720 -an -i - -threads 2 -preset slow "
                           "-crf 18 -pix_fmt yuv420p -vf vflip -y output.mp4";

        // Frame data to write into
        const size_t nbytes = 3 * WINDOW_WIDTH * WINDOW_HEIGHT;
        char *framedata = new char[nbytes];

        FILE *ffmpeg;

        if ( !(ffmpeg = popen(cmd.c_str(), "w") ) )
        {
            perror("Error using ffmpeg!");
            exit(-1);
        }
    #endif // MAKE_VIDEO

    // Main loop
    bool KernelBuildOk = false;
    cl_uint prevParticleCount = 0;
    cl_float simTime = 0.0f;
    cl_float waveTime = 0.0f;
    cl_float wavePos  = 0.0f;

    do
    {
        // Check file changes
        if (DetectResourceChanges() || renderer.UICmd_ResetSimulation)
        {
            // Reading the configuration file
            LoadParameters(getPathForScenario("dam_coarse.par"));

            // Check if particle count changed
            if ((prevParticleCount != Params.particleCount) || renderer.UICmd_ResetSimulation || Params.resetSimOnChange)
            {
                // Store new particle count
                prevParticleCount = Params.particleCount;

                // Generate shared buffer
                simulation.mSharingYinBufferID = renderer.createSharingBuffer(Params.particleCount * sizeof(cl_float4));
                simulation.mSharingYangBufferID = renderer.createSharingBuffer(Params.particleCount * sizeof(cl_float4));

                // Init buffers
                simulation.InitBuffers();
            }

            // Reset grid
            simulation.InitCells();

            // Init kernels
            KernelBuildOk = simulation.InitKernels();

            // Reset wavee
            waveTime = 0.0f;

            // Turn off sim reset request
            renderer.UICmd_ResetSimulation = false;
        }

        // Make sure that kernels are valid
        if (!KernelBuildOk)
            continue;

        // Generate waves
        if (renderer.UICmd_GenerateWaves)
        {
            // Wave consts
            const cl_float wave_push_length = Params.waveGenAmp * (Params.xMax - Params.xMin);

            // Update the wave position
            float t = Params.waveGenFreq * waveTime;
            wavePos = (1 - cos(2.0f * M_PI * pow(fmod(t, 1.0f), Params.waveGenDuty))) * wave_push_length / 2.0f;

            // Update wave running time
            if (!renderer.UICmd_PauseSimulation)
                waveTime += Params.timeStep;
        }
        else
        {
            waveTime = 0.0f;
        }

		// Load simulation settings
		simulation.bPauseSim        = renderer.UICmd_PauseSimulation;
		simulation.bReadFriendsList = renderer.UICmd_FriendsHistogarm;
		simulation.fWavePos         = wavePos;

        // Sub frames
        for (cl_uint i = 0; i < Params.subSteps; i++)
        {
            // Execute simulation
            simulation.Step();

            // Incremenent time
            if (!renderer.UICmd_PauseSimulation)
                simTime += Params.timeStep;
        }

        // Visualize particles
        renderer.visualizeParticles();

        renderer.checkInput();

        #if defined(MAKE_VIDEO)
            glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, framedata);
            fwrite(framedata, 1, nbytes, ffmpeg);
        #endif
    }
    while (true);

    #if defined(MAKE_VIDEO)
        pclose(ffmpeg);
    #endif
}
