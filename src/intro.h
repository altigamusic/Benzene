#pragma once
#include <windows.h>
#include "ext.h"

#ifdef EDITOR
extern int windowWidth;
extern int windowHeight;
extern int viewportWidth;
extern int viewportHeight;

#define WINDOW_WIDTH windowWidth
#define WINDOW_HEIGHT windowHeight
#define VIEWPORT_WIDTH viewportWidth
#define VIEWPORT_HEIGHT viewportHeight
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