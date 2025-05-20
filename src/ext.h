//--------------------------------------------------------------------------//
// iq / rgba  .  tiny codes  .  2008/2015                                   //
//--------------------------------------------------------------------------//

#ifndef _EXTENSIONS_H_
#define _EXTENSIONS_H_

#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "glext.h"

#ifndef DEBUG
#define NUMFUNCTIONS 4
#else
#define NUMFUNCTIONS 23
#endif

extern void* myglfunc[NUMFUNCTIONS];

#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)myglfunc[0])
#define glUseProgram ((PFNGLUSEPROGRAMPROC)myglfunc[1])
#define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC)myglfunc[2])
#define glUniform1f ((PFNGLUNIFORM1FPROC)myglfunc[3])

#ifdef DEBUG
#define glGenProgramPipelines ((PFNGLGENPROGRAMPIPELINESPROC)myglfunc[4])
#define glBindProgramPipeline ((PFNGLBINDPROGRAMPIPELINEPROC)myglfunc[5])
#define glUseProgramStages ((PFNGLUSEPROGRAMSTAGESPROC)myglfunc[6])
#define glProgramUniform4fv ((PFNGLPROGRAMUNIFORM4FVPROC)myglfunc[7])
#define glGenFramebuffers ((PFNGLGENFRAMEBUFFERSPROC)myglfunc[8])
#define glBindFramebuffer ((PFNGLBINDFRAMEBUFFERPROC)myglfunc[9])
#define glFramebufferTexture2D ((PFNGLFRAMEBUFFERTEXTURE2DPROC)myglfunc[10])
#define glCreateProgram ((PFNGLCREATEPROGRAMPROC)myglfunc[11])
#define glAttachShader ((PFNGLATTACHSHADERPROC)myglfunc[12])
#define glLinkProgram ((PFNGLLINKPROGRAMPROC)myglfunc[13])
#define glCreateShader ((PFNGLCREATESHADERPROC)myglfunc[14])
#define glShaderSource ((PFNGLSHADERSOURCEPROC)myglfunc[15])
#define glCompileShader ((PFNGLCOMPILESHADERPROC)myglfunc[16])
#define glActiveTexture ((PFNGLACTIVETEXTUREPROC)myglfunc[17])
#define glGetProgramiv ((PFNGLGETPROGRAMIVPROC)myglfunc[18])
#define glGetProgramInfoLog ((PFNGLGETPROGRAMINFOLOGPROC)myglfunc[19])
#define glCheckFramebufferStatus ((PFNGLCHECKFRAMEBUFFERSTATUSPROC)myglfunc[20])
#define glGetShaderiv ((PFNGLGETSHADERIVPROC)myglfunc[21])
#define glGetShaderInfoLog ((PFNGLGETSHADERINFOLOGPROC)myglfunc[22])
#endif

#define glUniform1i ((PFNGLUNIFORM1IPROC)wglGetProcAddress("glUniform1i"))
#define glBlitFramebuffer ((PFNGLBLITFRAMEBUFFERPROC)wglGetProcAddress("glBlitFramebuffer"))
#define glDeleteProgram ((PFNGLDELETEPROGRAMPROC)wglGetProcAddress("glDeleteProgram"))

// init
int EXT_Init(void);

#endif
