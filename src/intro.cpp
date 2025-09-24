#include <windows.h>
#include <gl/GL.h>

#pragma region External Defines
void updateUniforms(long timeInMs);
#pragma endregion

void initIntro()
{
    // Insert code here
}

void introLoop(long timeInMs)
{
    updateUniforms(timeInMs);

    // Insert code here - access to uniforms via glGetUniformfv

    glRects(-1, -1, 1, 1);
}
