#include "../release.h"

GLuint UNIFORMS[4] = {0};
GLuint timeUniformLocation = 0;

Keyframe keyframes0[] = {{0, 1.000000f, 0, 0}, {4321, 1.000000f, 0, 0}, {6522, 0.180000f, 3, 4}};
Keyframe keyframes2[] = {{0, 0.000000f, 0, 0}, {4321, 0.000000f, 0, 0}, {6522, 0.670000f, 3, 5}};
Keyframe keyframes3[] = {{0, 0.000000f, 0, 0}, {4321, 0.000000f, 0, 0}, {6522, 0.345000f, 3, 5}};
Keyframe keyframes4[] = {{0, 0.000000f, 0, 0}, {4321, 0.000000f, 0, 0}, {6522, 0.670000f, 3, 6}};
Keyframe keyframes5[] = {{0, 0.000000f, 0, 0}, {4321, 0.000000f, 0, 0}, {6522, 0.345000f, 3, 6}};

void locateUniforms(GLuint program) {
    timeUniformLocation = glGetUniformLocation(program, VAR__t);
    UNIFORMS[0] = glGetUniformLocation(program, VAR_rad);
    UNIFORMS[1] = glGetUniformLocation(program, VAR_offset);
    UNIFORMS[2] = glGetUniformLocation(program, VAR_p1);
    UNIFORMS[3] = glGetUniformLocation(program, VAR_p2);
}

void updateUniforms(float time) {
    glUniform1f(timeUniformLocation, time / 1000.0f);
    glUniform1f(UNIFORMS[0], valueAtTime(time, keyframes0, 3));
    glUniform1f(UNIFORMS[1], 0.0f);
    glUniform2f(UNIFORMS[2], valueAtTime(time, keyframes2, 3),valueAtTime(time, keyframes3, 3));
    glUniform2f(UNIFORMS[3], valueAtTime(time, keyframes4, 3),valueAtTime(time, keyframes5, 3));
}
