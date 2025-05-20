// #define USEDSOUND
// #define CLEANDESTROY          // destroy stuff (windows, glContext, ...)
#define XRES 1920
#define YRES 1080

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <GL/gl.h>
#include <fstream>
#include <math.h>
#include <mmreg.h>
#include <mmsystem.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "ext.h"
#include "intro.h"

extern const char* fragmentShaderSource;

static const PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 32, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0};

static DEVMODE screenSettings = {{0},
#if _MSC_VER < 1400
    0, 0, 148, 0, 0x001c0000, {0}, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0}, 0, 32, XRES, YRES, 0, 0,
#else
	0,
	0,
	156,
	0,
	0x001c0000,
	{0},
	0,
	0,
	0,
	0,
	0,
	{0},
	0,
	32,
	XRES,
	YRES,
	{0},
	0,
#endif
#if (WINVER >= 0x0400)
    0, 0, 0, 0, 0, 0,
#if (WINVER >= 0x0500) || (_WIN32_WINNT >= 0x0400)
    0, 0
#endif
#endif
};

#ifdef __cplusplus
extern "C"
{
#endif
    int _fltused = 0;
#ifdef __cplusplus
}
#endif

SUsample sound_buffer[SU_LENGTH_IN_SAMPLES * SU_CHANNEL_COUNT];
HWAVEOUT wave_out_handle;
WAVEFORMATEX wave_format = {
#ifdef SU_SAMPLE_FLOAT
    WAVE_FORMAT_IEEE_FLOAT,
#else
    WAVE_FORMAT_PCM,
#endif
    SU_CHANNEL_COUNT, SU_SAMPLE_RATE, SU_SAMPLE_RATE* SU_SAMPLE_SIZE* SU_CHANNEL_COUNT, SU_SAMPLE_SIZE* SU_CHANNEL_COUNT,
    SU_SAMPLE_SIZE * 8, 0};
WAVEHDR wave_header = {(LPSTR)sound_buffer, SU_LENGTH_IN_SAMPLES* SU_SAMPLE_SIZE* SU_CHANNEL_COUNT, 0, 0, WHDR_PREPARED, 0, 0, 0};
MMTIME mmtime = {TIME_SAMPLES, 0};

//----------------------------------------------------------------------------

#ifdef REL_DEBUG
#define RETURN_VALUE 0
int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
#define RETURN_VALUE
void entrypoint(void)
#endif
{
    // full screen
    // if (ChangeDisplaySettings(&screenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
    //	return;
    ShowCursor(0);
    // create window
    HWND hWnd = CreateWindow("static", 0, WS_POPUP | WS_VISIBLE | WS_MAXIMIZE, 0, 0, 0, 0, 0, 0, 0, 0);
    if (!hWnd) return RETURN_VALUE;
    HDC hDC = GetDC(hWnd);
    // initalize opengl
    if (!SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd)) return RETURN_VALUE;
    wglMakeCurrent(hDC, wglCreateContext(hDC));

    if (!EXT_Init()) return RETURN_VALUE;

    // init intro
    if (!initIntro()) return RETURN_VALUE;

    initFragmentShader(fragmentShaderSource);

    // Music
    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)su_render_song, sound_buffer, 0, 0);

    // We render in the background while playing already. Fortunately,
    // Windows is slow with the calls below, so we're not worried that
    // we don't have enough samples ready before the track starts.
    waveOutOpen(&wave_out_handle, WAVE_MAPPER, &wave_format, 0, 0, CALLBACK_NULL);
    waveOutWrite(wave_out_handle, &wave_header, sizeof(wave_header));

    // play intro
    long t = 0;
    long frame = 0;
    long to = timeGetTime();

    do
    {
        // 60FPS
        t = frame++ * 1000 / 60;

        if (t < 128000)
        {
            introLoop(min(15500, t % 16000),
                {
                    0, 0, t / 16000, {0, 0, 0},
                         {0, 0, 0},
                         {0, 0, 0},
                         {0, 0, 0}
            },
                false);
        }
        else
        {
            // Scene 8 is the last one so it can remain afterwards
            introLoop(t - 128000,
                {
                    0, 0, 8, {0, 0, 0},
                       {0, 0, 0},
                       {0, 0, 0},
                       {0, 0, 0}
            },
                false);
        }
        wglSwapLayerBuffers(hDC, WGL_SWAP_MAIN_PLANE); // SwapBuffers( hDC );
    } while (t < (150000));

#ifdef CLEANDESTROY
    sndPlaySound(0, 0);
    ChangeDisplaySettings(0, 0);
    ShowCursor(1);
#endif

    ExitProcess(0);
    return RETURN_VALUE;
}
