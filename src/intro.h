#pragma once
#include <windows.h>
#include "ext.h"

#ifdef EDITOR
#include "window_renderer.h"
extern WindowRenderer windowRenderer;

#define WINDOW_WIDTH windowRenderer.windowWidth
#define WINDOW_HEIGHT windowRenderer.windowHeight
#define VIEWPORT_WIDTH windowRenderer.viewportWidth
#define VIEWPORT_HEIGHT windowRenderer.viewportHeight
#else
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define VIEWPORT_WIDTH 800
#define VIEWPORT_HEIGHT 600
#endif

void initIntro(GLuint program);
void introLoop(float ftime);

// Externally defined
void updateUniforms(float ftime);