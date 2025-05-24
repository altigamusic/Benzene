#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "glext.h"
#include "ext.h"
#ifdef LINUX
#include <GL/glx.h>
#endif

static char* strs[] = {
    "glCreateShaderProgramv",
    "glUseProgram",
    "glGetUniformLocation",
    "glGenProgramPipelines",
    "glBindProgramPipeline",
    "glUseProgramStages",
    "glProgramUniform4fv",
    "glGenFramebuffers",
    "glBindFramebuffer",
    "glFramebufferTexture2D",
    "glCreateProgram",
    "glAttachShader",
    "glLinkProgram",
    "glCreateShader",
    "glShaderSource",
    "glCompileShader",
    "glActiveTexture",
    "glGetProgramiv",
    "glGetProgramInfoLog",
    "glCheckFramebufferStatus",
    "glGetShaderiv",
    "glGetShaderInfoLog",
    "glBlitFramebuffer",
    "glDeleteProgram",
    "glUniform1i",
    "glUniform1f",
    "glUniform2f",
    "glUniform3f",
    "glUniform4f",
};

void* myglfunc[NUM_FUNCTIONS];

int EXT_Init(void)
{
    for (int i = 0; i < NUM_FUNCTIONS; i++)
    {
#ifdef WINDOWS
        myglfunc[i] = wglGetProcAddress(strs[i]);
#endif
#ifdef LINUX
        myglfunc[i] = glXGetProcAddress((const unsigned char*)strs[i]);
#endif
        if (!myglfunc[i]) return 0;
    }
    return 1;
}
