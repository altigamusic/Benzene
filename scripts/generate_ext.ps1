$functions = Get-Content "src/glFunctions.txt" -Encoding UTF8 | Where-Object { !$_.StartsWith("#") }
[array]$releaseFunctions = $functions | Where-Object { !$_.StartsWith("+") }
[array]$debugFunctions = $functions | Where-Object { $_.StartsWith("+") } | ForEach-Object { $_ -replace "^\+", "" }

$releaseArrayString = ($releaseFunctions | ForEach-Object { "`"$($_)`"," }) -join "`r`n    "
$debugArrayString = ($debugFunctions | ForEach-Object { "`"$($_)`"," }) -join "`r`n    "
$releaseDefines = "";
$debugDefines = "";

for ($i = 0; $i -lt $releaseFunctions.Count; $i++)
{
    $releaseDefines += "#define $($releaseFunctions[$i]) ((PFN$($releaseFunctions[$i].ToUpper())PROC)myglfunc[$i])`r`n"
}
for ($j = 0; $j -lt $debugFunctions.Count; $j++)
{
    $debugDefines += "#define $($debugFunctions[$j]) ((PFN$($debugFunctions[$j].ToUpper())PROC)myglfunc[$($i+$j)])`r`n"
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

#ifdef DEBUG
#define NUM_FUNCTIONS $($functions.Count)
#else
#define NUM_FUNCTIONS $($releaseFunctions.Count)
#endif

extern void* myglfunc[NUM_FUNCTIONS];

$releaseDefines
#ifdef DEBUG
$debugDefines
#endif

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
    $releaseArrayString
#ifdef DEBUG
    $debugArrayString
#endif
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