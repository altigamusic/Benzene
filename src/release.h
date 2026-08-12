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
    unsigned char interpolation;
#ifndef DEFAULT_INTERPOLATION_FACTOR
    float interpolationFactor;
#endif
};

inline float valueAtTime(float time, kf_time_t* times, float* values, unsigned char* interpolations,
#ifndef DEFAULT_INTERPOLATION_FACTOR
    char* tensions,
#endif
    int offset, int keyframeCount)
{
    times += offset;
    values += offset;
    interpolations += offset;
#ifndef DEFAULT_INTERPOLATION_FACTOR
    tensions += offset;
#endif

    int i = 0;
    while (times[i] <= time && i < keyframeCount)
        i++;

    if (i == keyframeCount) return values[keyframeCount - 1];

    // This assumes a keyframe at 0
    float t = ((time - times[i - 1]) / (times[i] - times[i - 1]));
#ifdef DEFAULT_INTERPOLATION_FACTOR
    float a = DEFAULT_INTERPOLATION_FACTOR;
#else
    float a = ((float)tensions[i]) / 6.0f;
#endif

    switch (interpolations[i])
    {
    case 1: // Step
        t = 0;
        break;
#ifdef INCLUDE_TONEMAP
    case 2: // Tonemap
        t = a > 0.f ? pow(t, a + 1.f) : 1.f - pow(1.f - t, 1.f - a);
        break;
#endif
#ifdef INCLUDE_GAIN
    case 3: // Gain
        t = (t < 0.5) ? 0.5f * pow(2.0f * t, a) : 1.0f - 0.5f * pow(2.0f * (1.0f - t), a);
        break;
#endif
    default: // Linear
        // No action required
        break;
    }

    return values[i - 1] + (values[i] - values[i - 1]) * t;
}

static const float valueAtTimeAsmExponent = 0.;
static const float valueAtTimeAsmHalf = 0.5f;

__declspec(naked) inline float valueAtTimeAsm(float time, Keyframe* keyframes, int offset, int keyframeCount)
{
    __asm
    {
        push ebx
        mov ebx, DWORD PTR [esp + 12]
        mov eax, DWORD PTR [esp + 16]
        lea eax, [eax + eax * 8]
        add ebx, eax
        mov ecx, DWORD PTR [esp + 20]
        fld DWORD PTR [esp + 8]

scanLoop:
        ficom DWORD PTR [ebx]
        fnstsw ax
        sahf
        jb foundRange
        add ebx, 9
        dec ecx
        jnz scanLoop

        fstp st(0)
        fld DWORD PTR [ebx - 5]
        pop ebx
        ret 12

foundRange:
        fild DWORD PTR [ebx - 9]
        fsubp st(1), st(0)
        mov eax, DWORD PTR [ebx]
        sub eax, DWORD PTR [ebx - 9]
        push eax
        fidiv DWORD PTR [esp]
        add esp, 4

        cmp BYTE PTR [ebx + 8], 1
        je stepInterpolation
#ifdef INCLUDE_GAIN
        cmp BYTE PTR [ebx + 8], 3
        jne finishInterpolation

        sub esp, 4
        fst DWORD PTR [esp]
        cmp DWORD PTR [esp], 3f000000h
        jb gainLowerHalf

        fstp st(0)
        fld1
        fsub DWORD PTR [esp]
        add esp, 4
        fadd st(0), st(0)
        sub esp, 16
        fstp QWORD PTR [esp]
        fld DWORD PTR valueAtTimeAsmExponent
        fstp QWORD PTR [esp + 8]
        call pow
        add esp, 16
        fmul DWORD PTR valueAtTimeAsmHalf
        fld1
        fsubrp st(1), st(0)
        jmp finishInterpolation

gainLowerHalf:
        add esp, 4
        fadd st(0), st(0)
        sub esp, 16
        fstp QWORD PTR [esp]
        fld DWORD PTR valueAtTimeAsmExponent
        fstp QWORD PTR [esp + 8]
        call pow
        add esp, 16
        fmul DWORD PTR valueAtTimeAsmHalf
#endif

finishInterpolation:
        fld DWORD PTR [ebx + 4]
        fsub DWORD PTR [ebx - 5]
        fmulp st(1), st(0)
        fadd DWORD PTR [ebx - 5]
        pop ebx
        ret 12

stepInterpolation:
        fstp st(0)
        fldz
        jmp finishInterpolation
    }
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