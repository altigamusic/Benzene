#ifndef EXT_H
#define EXT_H
#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "glext.h"

#define NUM_FUNCTIONS 27

extern void* myglfunc[NUM_FUNCTIONS];

#define glCreateShaderProgramv ((PFNGLCREATESHADERPROGRAMVPROC)myglfunc[0])
#define glUseProgram ((PFNGLUSEPROGRAMPROC)myglfunc[1])
#define glGetUniformLocation ((PFNGLGETUNIFORMLOCATIONPROC)myglfunc[2])
#define glUniform1f ((PFNGLUNIFORM1FPROC)myglfunc[3])
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
#define glBlitFramebuffer ((PFNGLBLITFRAMEBUFFERPROC)myglfunc[23])
#define glDeleteProgram ((PFNGLDELETEPROGRAMPROC)myglfunc[24])
#define glUniform1i ((PFNGLUNIFORM1IPROC)myglfunc[25])
#define glUniform2f ((PFNGLUNIFORM2FPROC)myglfunc[26])

int EXT_Init(void);

#endif
