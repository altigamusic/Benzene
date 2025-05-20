#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include "intro.h"
#include "ext.h"
#include "glext.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <windows.h>

static GLuint fragmentShaderProgram = 0;
static GLuint timeLocation;

void loadFragmentShader(const char* fragmentShaderSource)
{
    int length;
    char infoLog[500];

    if (fragmentShaderProgram != 0)
    {
        glDeleteProgram(fragmentShaderProgram);
        fragmentShaderProgram = 0;
    }

    fragmentShaderProgram = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragmentShaderSource);

    glGetProgramInfoLog(fragmentShaderProgram, 500, &length, infoLog);

    if (length > 0) MessageBox(0, infoLog, "Result:", MB_OK | MB_ICONEXCLAMATION);

    timeLocation = glGetUniformLocation(fragmentShaderProgram, "_t");

    glUseProgram(fragmentShaderProgram);
}

void initIntro() {}

void introLoop(long timeInMs)
{
    const float ftime = 0.001f * (float)timeInMs;

    glUniform1f(timeLocation, ftime);

    glRects(-1, -1, 1, 1);
}
