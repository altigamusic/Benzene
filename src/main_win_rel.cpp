// #define USEDSOUND
// #define CLEANDESTROY          // destroy stuff (windows, glContext, ...)
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <math.h>
#include <GL/gl.h>
#include <mmsystem.h>
#include "ext.h"
#include "release.h"
#include "intro.h"
#include "generated/release_config.h"

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

    GLuint program = initFragmentShader(fragmentShaderSource);
    initIntro(program);

    // play intro
    float t = 0;
    long frame = 0;
    long to = timeGetTime();

    do {
        t = (timeGetTime() - to) * BPM / 60000.0f;
        
        introLoop(t);

        wglSwapLayerBuffers(hDC, WGL_SWAP_MAIN_PLANE); // SwapBuffers( hDC );
    } while (!GetAsyncKeyState(VK_ESCAPE) && t < DEMO_LENGTH);

#ifdef CLEANDESTROY
    sndPlaySound(0, 0);
    ChangeDisplaySettings(0, 0);
    ShowCursor(1);
#endif

    ExitProcess(0);
    return RETURN_VALUE;
}
