#include "intro.h"

void initIntro(GLuint program)
{
    // Insert code here
}

void introLoop(const float ftime)
{
    updateUniforms(ftime);

    // Insert code here - access to uniforms via glGetUniformfv

    glRects(-1, -1, 1, 1);
}
