#pragma once
#include <windows.h>
#include "ext.h"

#ifdef EDITOR
#include "editor/window_renderer.h"
extern WindowRenderer windowRenderer;

#define WINDOW_WIDTH windowRenderer.windowWidth
#define WINDOW_HEIGHT windowRenderer.windowHeight
#define VIEWPORT_WIDTH windowRenderer.viewportWidth
#define VIEWPORT_HEIGHT windowRenderer.viewportHeight
#else
#include "generated/release_config.h"
#define WINDOW_WIDTH XRES
#define WINDOW_HEIGHT YRES
#define VIEWPORT_WIDTH XRES
#define VIEWPORT_HEIGHT YRES
#endif

void initIntro(GLuint program);
void introLoop(float ftime);

// Externally defined
void updateUniforms(float ftime);