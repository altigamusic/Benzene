#include "intro.h"

void initIntro(GLuint program)
{
    // Insert code here
}

void introLoop(long timeInMs)
{
    updateUniforms(timeInMs);

    // Insert code here - access to uniforms via glGetUniformfv

    glRects(-1, -1, 1, 1);
}
