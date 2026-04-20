#ifndef EXT_H
#define EXT_H
#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "glext.h"

#ifdef DEBUG
#define NUM_FUNCTIONS 30
#else
#define NUM_FUNCTIONS 19
#endif

extern void* myglfunc[NUM_FUNCTIONS];

#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)myglfunc[0])
#define glUseProgram ((PFNGLUSEPROGRAMPROC)myglfunc[1])
#define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC)myglfunc[2])
#define glGenProgramPipelines ((PFNGLGENPROGRAMPIPELINESPROC)myglfunc[3])
#define glBindProgramPipeline ((PFNGLBINDPROGRAMPIPELINEPROC)myglfunc[4])
#define glUseProgramStages ((PFNGLUSEPROGRAMSTAGESPROC)myglfunc[5])
#define glProgramUniform4fv ((PFNGLPROGRAMUNIFORM4FVPROC)myglfunc[6])
#define glCreateProgram ((PFNGLCREATEPROGRAMPROC)myglfunc[7])
#define glAttachShader ((PFNGLATTACHSHADERPROC)myglfunc[8])
#define glLinkProgram ((PFNGLLINKPROGRAMPROC)myglfunc[9])
#define glCreateShader ((PFNGLCREATESHADERPROC)myglfunc[10])
#define glShaderSource ((PFNGLSHADERSOURCEPROC)myglfunc[11])
#define glCompileShader ((PFNGLCOMPILESHADERPROC)myglfunc[12])
#define glActiveTexture ((PFNGLACTIVETEXTUREPROC)myglfunc[13])
#define glGetProgramiv ((PFNGLGETPROGRAMIVPROC)myglfunc[14])
#define glGetShaderiv ((PFNGLGETSHADERIVPROC)myglfunc[15])
#define glGetShaderInfoLog ((PFNGLGETSHADERINFOLOGPROC)myglfunc[16])
#define glBlitFramebuffer ((PFNGLBLITFRAMEBUFFERPROC)myglfunc[17])
#define glUniform1fv ((PFNGLUNIFORM1FVPROC)myglfunc[18])

#ifdef DEBUG
#define glUniform1f ((PFNGLUNIFORM1FPROC)myglfunc[19])
#define glUniform1i ((PFNGLUNIFORM1IPROC)myglfunc[20])
#define glUniform2f ((PFNGLUNIFORM2FPROC)myglfunc[21])
#define glUniform3f ((PFNGLUNIFORM3FPROC)myglfunc[22])
#define glUniform4f ((PFNGLUNIFORM4FPROC)myglfunc[23])
#define glGenFramebuffers ((PFNGLGENFRAMEBUFFERSPROC)myglfunc[24])
#define glBindFramebuffer ((PFNGLBINDFRAMEBUFFERPROC)myglfunc[25])
#define glFramebufferTexture2D ((PFNGLFRAMEBUFFERTEXTURE2DPROC)myglfunc[26])
#define glGetProgramInfoLog ((PFNGLGETPROGRAMINFOLOGPROC)myglfunc[27])
#define glCheckFramebufferStatus ((PFNGLCHECKFRAMEBUFFERSTATUSPROC)myglfunc[28])
#define glDeleteProgram ((PFNGLDELETEPROGRAMPROC)myglfunc[29])

#endif

int EXT_Init(void);

#endif
