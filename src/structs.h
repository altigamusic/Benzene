#pragma once
#include <string>

enum class UniformType
{
    Float,
    Vec2,
    Vec3,
    Vec4,
    Int,
    Bool,
    Color
};

union UniformValue
{
    float f;
    float v2[2];
    float v3[3];
    float v4[4];
    int i;
    bool b;
};

typedef struct _Uniform
{
    std::string name;
    UniformType type;
    GLuint location;
    UniformValue value;
} Uniform;