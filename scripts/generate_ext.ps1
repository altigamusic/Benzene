$functions = Get-Content "src/glFunctions.txt" -Encoding UTF8


$arrayString = ($functions | ForEach-Object { "`"$($_)`"," }) -join "`r`n    "
$defines = "";

for ($i = 0; $i -lt $functions.Count; $i++)
{
    $defines += "#define $($functions[$i]) ((PFN$($functions[$i].ToUpper())PROC)myglfunc[$i])`r`n"
}

$hFile = @"
#ifndef EXT_H
#define EXT_H
#ifdef WINDOWS
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "glext.h"

#define NUM_FUNCTIONS $($functions.Count)

extern void* myglfunc[NUM_FUNCTIONS];

$defines
int EXT_Init(void);

#endif
"@;

$cppFile = @"
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
    $arrayString
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
"@;

Set-Content -Path "src/ext.h" -Value $hFile;

Set-Content -Path "src/ext.cpp" -Value $cppFile;