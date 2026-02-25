#pragma once
#include "ext.h"
#include "generated/shader.inl"
#include <math.h>
#include "generated/release_config.h"

#pragma function(pow)

struct Keyframe
{
    int time;
    float value;
    int interpolation;
#ifndef DEFAULT_INTERPOLATION_FACTOR
    float interpolationFactor;
#endif
};

inline float valueAtTime(float time, Keyframe* keyframes, int keyframeCount)
{
    int i = 0;
    while (keyframes[i].time <= time && i < keyframeCount)
        i++;

    if (i == keyframeCount) return keyframes[keyframeCount - 1].value;

    // This assumes a keyframe at 0
    float t = ((time - keyframes[i - 1].time) / (keyframes[i].time - keyframes[i - 1].time));
#ifdef DEFAULT_INTERPOLATION_FACTOR
    float a = DEFAULT_INTERPOLATION_FACTOR;
#else
    float a = keyframes[i].interpolationFactor;
#endif

    switch (keyframes[i].interpolation)
    {
    case 1: // Step
        t = 0;
        break;
    case 2: // Tonemap
        t = a > 0.f ? pow(t, a + 1.f) : 1.f - pow(1.f - t, 1.f - a);
        break;
    case 3: // Gain
        t = (t < 0.5) ? 0.5f * pow(2.0f * t, a) : 1.0f - 0.5f * pow(2.0f * (1.0f - t), a);
        break;
    default: // Linear
        // No action required
        break;
    }

    return keyframes[i - 1].value + (keyframes[i].value - keyframes[i - 1].value) * t;
}

void locateUniforms(GLuint program);
void updateUniforms(long time);

inline GLuint initFragmentShader(const char* fragmentShaderSource)
{
    GLuint program = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragmentShaderSource);

#ifdef DEBUG
    int length;
    char infoLog[500];
    glGetProgramInfoLog(program, 500, &length, infoLog);

    if (length > 0)
    {
        MessageBoxA(NULL, infoLog, "OpenGL Debug", MB_ICONWARNING);
    }
#endif

    GLuint pipeline;
    glGenProgramPipelines(1, &pipeline);
    glBindProgramPipeline(pipeline);
    glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, program);
    glUseProgram(program);
    locateUniforms(program);

    return program;
}