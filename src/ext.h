#ifndef EXT_H
#define EXT_H
#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "glext.h"

#define NUM_FUNCTIONS 29

extern void* myglfunc[NUM_FUNCTIONS];

#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)myglfunc[0])
#define glUseProgram ((PFNGLUSEPROGRAMPROC)myglfunc[1])
#define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC)myglfunc[2])
#define glGenProgramPipelines ((PFNGLGENPROGRAMPIPELINESPROC)myglfunc[3])
#define glBindProgramPipeline ((PFNGLBINDPROGRAMPIPELINEPROC)myglfunc[4])
#define glUseProgramStages ((PFNGLUSEPROGRAMSTAGESPROC)myglfunc[5])
#define glProgramUniform4fv ((PFNGLPROGRAMUNIFORM4FVPROC)myglfunc[6])
#define glGenFramebuffers ((PFNGLGENFRAMEBUFFERSPROC)myglfunc[7])
#define glBindFramebuffer ((PFNGLBINDFRAMEBUFFERPROC)myglfunc[8])
#define glFramebufferTexture2D ((PFNGLFRAMEBUFFERTEXTURE2DPROC)myglfunc[9])
#define glCreateProgram ((PFNGLCREATEPROGRAMPROC)myglfunc[10])
#define glAttachShader ((PFNGLATTACHSHADERPROC)myglfunc[11])
#define glLinkProgram ((PFNGLLINKPROGRAMPROC)myglfunc[12])
#define glCreateShader ((PFNGLCREATESHADERPROC)myglfunc[13])
#define glShaderSource ((PFNGLSHADERSOURCEPROC)myglfunc[14])
#define glCompileShader ((PFNGLCOMPILESHADERPROC)myglfunc[15])
#define glActiveTexture ((PFNGLACTIVETEXTUREPROC)myglfunc[16])
#define glGetProgramiv ((PFNGLGETPROGRAMIVPROC)myglfunc[17])
#define glGetProgramInfoLog ((PFNGLGETPROGRAMINFOLOGPROC)myglfunc[18])
#define glCheckFramebufferStatus ((PFNGLCHECKFRAMEBUFFERSTATUSPROC)myglfunc[19])
#define glGetShaderiv ((PFNGLGETSHADERIVPROC)myglfunc[20])
#define glGetShaderInfoLog ((PFNGLGETSHADERINFOLOGPROC)myglfunc[21])
#define glBlitFramebuffer ((PFNGLBLITFRAMEBUFFERPROC)myglfunc[22])
#define glDeleteProgram ((PFNGLDELETEPROGRAMPROC)myglfunc[23])
#define glUniform1i ((PFNGLUNIFORM1IPROC)myglfunc[24])
#define glUniform1f ((PFNGLUNIFORM1FPROC)myglfunc[25])
#define glUniform2f ((PFNGLUNIFORM2FPROC)myglfunc[26])
#define glUniform3f ((PFNGLUNIFORM3FPROC)myglfunc[27])
#define glUniform4f ((PFNGLUNIFORM4FPROC)myglfunc[28])

int EXT_Init(void);

#endif
