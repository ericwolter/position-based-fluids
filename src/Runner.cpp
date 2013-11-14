#include "Runner.hpp"
#include "io/Parameters.hpp"

#define _USE_MATH_DEFINES
#include <math.h>

#include <sstream>
#include <cstdio>
#include <functional>
#include <numeric>

#if defined(MAKE_VIDEO)
#include <unistd.h>
#endif // MAKE_VIDEO

#include <GLFW/glfw3.h>

using std::string;
using std::ostringstream;
using std::cout;
using std::endl;


static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

bool bFirstTime = true;

bool Runner::ResourceChanged() const
{
	bool Prev = bFirstTime;
	bFirstTime = false;
	return Prev;
}

void Runner::run(Simulation& simulation, CVisual& renderer) const
{
	cl_uint prevParticleCount = 0;
    cl_float wave = 0.0f;
    bool shouldGenerateWaves = false;

    // Init render (background, camera etc...)
	const cl_float4 sizesMin = {{ Params.xMin, Params.yMin, Params.zMin, 0 }};
    const cl_float4 sizesMax = {{ Params.xMax, Params.yMax, Params.zMax, 0 }};
    renderer.initSystemVisual(simulation, sizesMin, sizesMax);
    renderer.initParticlesVisual();

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
    cl_float time = 0.0f;
    do
    {
		// Check file changes
		if (ResourceChanged())
		{
			// Reading the configuration file
			Params.LoadParameters(getPathForScenario("dam_coarse.par"));

			// Check if particle count changed
			if (prevParticleCount != Params.particleCount)
			{
				// Store new particle count
				prevParticleCount = Params.particleCount;

				// Generate shared buffer
				simulation.mSharingBufferID = renderer.createSharingBuffer(Params.particleCount * sizeof(cl_float4));

			    // Init buffers
				simulation.InitBuffers();
			}

			// Reset grid
			#if defined(USE_LINKEDCELL)
				simulation.InitCells();
			#endif // USE_LINKEDCELL

			// Init kernels
			simulation.InitKernels();
		}

        simulation.Step();

        time += Params.timeStepLength;

        if (shouldGenerateWaves)
        {
            static const cl_float wave_push_length = (sizesMax.s[0] - sizesMin.s[0]) / 4.0f;
            static const cl_float wave_frequency = 0.70f;
            static const cl_float wave_start = 0;

            cl_float waveValue = sin(2.0f * M_PI * wave_frequency
                                     * wave + wave_start)
                                 * wave_push_length / 2.0f
                                 + wave_push_length / 2.0f;

            float t = wave_frequency * wave + wave_start;
            waveValue = (1 - cos(2.0f * M_PI * pow(fmod(t, 1.0f), 3.0f))) * wave_push_length / 2.0f;

            simulation.setWaveGenerator(waveValue);
            wave += Params.timeStepLength;
        }

        // Visualize particles
        renderer.visualizeParticles();
        renderer.checkInput(shouldGenerateWaves);

        if ( !shouldGenerateWaves )
            wave = 0.0f;

#if defined(MAKE_VIDEO)
        glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, framedata);
        fwrite(framedata, 1, nbytes, ffmpeg);
#endif

#if defined(USE_DEBUG)
        cout << "Time: " << time << endl;
#endif // USE_DEBUG

    }
    while (time <= Params.timeEnd);

#if defined(MAKE_VIDEO)
    pclose(ffmpeg);
#endif

}
