#ifndef _INTRO_H_
#define _INTRO_H_

#define RES_X 800
#define RES_Y 600

#include <math.h>

void loadFragmentShader(const char* fragmentShaderSource);

void initIntro(void);
void introLoop(long time);

typedef struct _vec3
{
    float x;
    float y;
    float z;

    inline _vec3 operator+(_vec3 a) { return {a.x + x, a.y + y, a.z + z}; }

    inline _vec3 operator+(float s) { return {x + s, y + s, z + s}; }

    inline _vec3 operator-(_vec3 a) { return {a.x - x, a.y - y, a.z - z}; }

    inline _vec3 operator-(float s) { return {x - s, y - s, z - s}; }

    inline _vec3 operator*(_vec3 a) { return {a.x * x, a.y * y, a.z * z}; }

    inline _vec3 operator*(float s) { return {x * s, y * s, z * s}; }

    inline void operator*=(_vec3 a)
    {
        x *= a.x;
        y *= a.y;
        z *= a.z;
    }

    inline void operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
    }

    inline struct _vec3 cross(struct _vec3 other)
    {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }

    inline _vec3 normalize()
    {
        // Normalize
        return *this * (1.0f / sqrt(x * x + y * y + z * z));
    }
} vec3;

typedef struct _vec4
{
    float x;
    float y;
    float z;
    float w;
} vec4;

typedef struct _IntroParams
{
    float s0;
    float s1;
    float s2;
    float s3;
    float s4;
    float s5;
    int scene;
    vec3 camera;
    vec3 target;
} IntroParams;

#endif
