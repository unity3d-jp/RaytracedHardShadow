#pragma once
#include "rthsHalf.h"

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

namespace rths {

constexpr float PI = 3.14159265358979323846264338327950288419716939937510f;
constexpr float DegToRad = PI / 180.0f;
constexpr float RadToDeg = 1.0f / (PI / 180.0f);

template<class T>
struct tvec2
{
    using scalar_t = T;
    static const int vector_length = 2;

    T x, y;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tvec2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const tvec2& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1] }; }
    template<class U> void assign(const tvec2<U>& v) { assign((const U*)&v); }

    static constexpr tvec2 zero() { return{ (T)0, (T)0 }; }
    static constexpr tvec2 one() { return{ (T)1, (T)1 }; }
    static constexpr tvec2 set(T v) { return{ v, v }; }
};

template<class T>
struct tvec3
{
    using scalar_t = T;
    static const int vector_length = 3;

    T x, y, z;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tvec3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const tvec3& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1], (T)v[2] }; }
    template<class U> void assign(const tvec3<U>& v) { assign((const U*)&v); }

    static constexpr tvec3 zero() { return{ (T)0, (T)0, (T)0 }; }
    static constexpr tvec3 one() { return{ (T)1, (T)1, (T)1 }; }
    static constexpr tvec3 set(T v) { return{ v, v, v }; }
};

template<class T>
struct tvec4
{
    using scalar_t = T;
    static const int vector_length = 4;

    T x, y, z, w;
    T& operator[](int i) { return ((T*)this)[i]; }
    const T& operator[](int i) const { return ((T*)this)[i]; }
    bool operator==(const tvec4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
    bool operator!=(const tvec4& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v) { *this = { (T)v[0], (T)v[1], (T)v[2], (T)v[3] }; }
    template<class U> void assign(const tvec4<U>& v) { assign((const U*)&v); }

    static constexpr tvec4 zero() { return{ (T)0, (T)0, (T)0, (T)0 }; }
    static constexpr tvec4 one() { return{ (T)1, (T)1, (T)1, (T)1 }; }
    static constexpr tvec4 set(T v) { return{ v, v, v, v }; }
};

template<class T>
struct tmat3x3
{
    using scalar_t = T;
    using vector_t = tvec3<T>;
    static const int vector_length = 9;

    tvec3<T> m[3];
    tvec3<T>& operator[](int i) { return m[i]; }
    const tvec3<T>& operator[](int i) const { return m[i]; }
    bool operator==(const tmat3x3& v) const { return memcmp(m, v.m, sizeof(*this)) == 0; }
    bool operator!=(const tmat3x3& v) const { return !((*this) == v); }

    template<class U> void assign(const U *v)
    {
        *this = { {
            { (T)v[0], (T)v[1], (T)v[2] },
            { (T)v[3], (T)v[4], (T)v[5] },
            { (T)v[6], (T)v[7], (T)v[8] }
        } };
    }
    template<class U> void assign(const tmat3x3<U>& v) { assign((U*)&v); }

    static constexpr tmat3x3 identity()
    {
        return{ {
            { T(1.0), T(0.0), T(0.0) },
            { T(0.0), T(1.0), T(0.0) },
            { T(0.0), T(0.0), T(1.0) },
        } };
    }
};

template<class T>
struct tmat3x4
{
    using scalar_t = T;
    using vector_t = tvec4<T>;
    static const int vector_length = 12;

    tvec4<T> m[3];
    tvec4<T>& operator[](int i) { return m[i]; }
    const tvec4<T>& operator[](int i) const { return m[i]; }
    bool operator==(const tmat3x4& v) const { return memcmp(m, v.m, sizeof(*this)) == 0; }
    bool operator!=(const tmat3x4& v) const { return !((*this) == v); }

    void assign(const T *v)
    {
        memcpy(this, v, sizeof(*this));
    }
    template<class U> void assign(const U *v)
    {
        *this = { {
            { (T)v[0], (T)v[1], (T)v[2], (T)v[3] },
            { (T)v[4], (T)v[5], (T)v[6], (T)v[7] },
            { (T)v[8], (T)v[9], (T)v[10],(T)v[11]}
        } };
    }
    template<class U> void assign(const tmat3x4<U>& v) { assign((U*)&v); }

    static constexpr tmat3x4 identity()
    {
        return{ {
            { (T)1, (T)0, (T)0, (T)0 },
            { (T)0, (T)1, (T)0, (T)0 },
            { (T)0, (T)0, (T)1, (T)0 }
        } };
    }
};

template<class T>
struct tmat4x4
{
    using scalar_t = T;
    using vector_t = tvec4<T>;
    static const int vector_length = 16;

    tvec4<T> m[4];
    tvec4<T>& operator[](int i) { return m[i]; }
    const tvec4<T>& operator[](int i) const { return m[i]; }
    bool operator==(const tmat4x4& v) const { return memcmp(m, v.m, sizeof(*this)) == 0; }
    bool operator!=(const tmat4x4& v) const { return !((*this) == v); }

    void assign(const T *v)
    {
        memcpy(this, v, sizeof(*this));
    }
    template<class U> void assign(const U *v)
    {
        *this = { {
            { (T)v[0], (T)v[1], (T)v[2], (T)v[3] },
            { (T)v[4], (T)v[5], (T)v[6], (T)v[7] },
            { (T)v[8], (T)v[9], (T)v[10],(T)v[11]},
            { (T)v[12],(T)v[13],(T)v[14],(T)v[15]}
        } };
    }
    template<class U> void assign(const tmat4x4<U>& v) { assign((U*)&v); }

    static constexpr tmat4x4 identity()
    {
        return{ {
            { (T)1, (T)0, (T)0, (T)0 },
            { (T)0, (T)1, (T)0, (T)0 },
            { (T)0, (T)0, (T)1, (T)0 },
            { (T)0, (T)0, (T)0, (T)1 },
        } };
    }
};

using int2 = tvec2<int>;
using int3 = tvec3<int>;
using int4 = tvec4<int>;
using unorm8x2 = tvec2<unorm8>;
using unorm8x3 = tvec3<unorm8>;
using unorm8x4 = tvec4<unorm8>;
using half2 = tvec2<half>;
using half3 = tvec3<half>;
using half4 = tvec4<half>;
#ifndef rthsTestImpl
using float2 = tvec2<float>;
using float3 = tvec3<float>;
using float4 = tvec4<float>;
using float3x3 = tmat3x3<float>;
using float3x4 = tmat3x4<float>;
using float4x4 = tmat4x4<float>;
#endif // rthsTestImpl


inline float3 operator-(const float3& l) { return{ -l.x, -l.y, -l.z }; }
inline float3 operator+(const float3& l, const float3& r) { return{ l.x + r.x, l.y + r.y, l.z + r.z }; }
inline float3 operator-(const float3& l, const float3& r) { return{ l.x - r.x, l.y - r.y, l.z - r.z }; }
inline float3 operator*(const float3& l, float r) { return{ l.x * r, l.y * r, l.z * r }; }
inline float3 operator/(const float3& l, float r) { return{ l.x / r, l.y / r, l.z / r }; }

inline int ceildiv(int v, int d) { return (v + (d - 1)) / d; }
inline float clamp(float v, float vmin, float vmax) { return std::min<float>(std::max<float>(v, vmin), vmax); }
inline float clamp01(float v) { return clamp(v, 0.0f, 1.0f); }
inline float clamp11(float v) { return clamp(v, -1.0f, 1.0f); }
inline float dot(const float3& l, const float3& r) { return l.x*r.x + l.y*r.y + l.z*r.z; }
inline float length_sq(const float3& v) { return dot(v, v); }
inline float length(const float3& v) { return sqrt(length_sq(v)); }
inline float3 normalize(const float3& v) { return v / length(v); }
inline float3 cross(const float3& l, const float3& r)
{
    return{
        l.y * r.z - l.z * r.y,
        l.z * r.x - l.x * r.z,
        l.x * r.y - l.y * r.x
    };
}

inline float4 to_float4(const float3& xyz, float w)
{
    return{ xyz.x, xyz.y, xyz.z, w };
}

#ifndef rthsTestImpl
inline float3x4 to_float3x4(const float4x4& v)
{
    // copy with transpose
    return float3x4{ {
        {v[0][0], v[1][0], v[2][0], v[3][0]},
        {v[0][1], v[1][1], v[2][1], v[3][1]},
        {v[0][2], v[1][2], v[2][2], v[3][2]},
    } };
}
#endif // rthsTestImpl

inline float3 extract_position(const float4x4& m)
{
    return (const float3&)m[3];
}
inline float3 extract_direction(const float4x4& m)
{
    return normalize((const float3&)m[2]);
}

inline float4x4 operator*(const float4x4 &a, const float4x4 &b)
{
    float4x4 c;
    const float *ap = &a[0][0];
    const float *bp = &b[0][0];
    float *cp = &c[0][0];
    float a0, a1, a2, a3;

    a0 = ap[0];
    a1 = ap[1];
    a2 = ap[2];
    a3 = ap[3];

    cp[0] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[1] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[2] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[3] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

    a0 = ap[4];
    a1 = ap[5];
    a2 = ap[6];
    a3 = ap[7];

    cp[4] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[5] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[6] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[7] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

    a0 = ap[8];
    a1 = ap[9];
    a2 = ap[10];
    a3 = ap[11];

    cp[8] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[9] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[10] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[11] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

    a0 = ap[12];
    a1 = ap[13];
    a2 = ap[14];
    a3 = ap[15];

    cp[12] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[13] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[14] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[15] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];
    return c;
}

inline float4x4 invert(const float4x4& x)
{
    float4x4 s{
        x[1][1] * x[2][2] - x[2][1] * x[1][2],
        x[2][1] * x[0][2] - x[0][1] * x[2][2],
        x[0][1] * x[1][2] - x[1][1] * x[0][2],
        0,

        x[2][0] * x[1][2] - x[1][0] * x[2][2],
        x[0][0] * x[2][2] - x[2][0] * x[0][2],
        x[1][0] * x[0][2] - x[0][0] * x[1][2],
        0,

        x[1][0] * x[2][1] - x[2][0] * x[1][1],
        x[2][0] * x[0][1] - x[0][0] * x[2][1],
        x[0][0] * x[1][1] - x[1][0] * x[0][1],
        0,

        0, 0, 0, 1,
    };

    auto r = x[0][0] * s[0][0] + x[0][1] * s[1][0] + x[0][2] * s[2][0];

    if (std::abs(r) >= 1) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                s[i][j] /= r;
            }
        }
    }
    else {
        auto mr = std::abs(r) / std::numeric_limits<float>::min();

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (mr > std::abs(s[i][j])) {
                    s[i][j] /= r;
                }
                else {
                    // error
                    return float4x4::identity();
                }
            }
        }
    }

    s[3][0] = -x[3][0] * s[0][0] - x[3][1] * s[1][0] - x[3][2] * s[2][0];
    s[3][1] = -x[3][0] * s[0][1] - x[3][1] * s[1][1] - x[3][2] * s[2][1];
    s[3][2] = -x[3][0] * s[0][2] - x[3][1] * s[1][2] - x[3][2] * s[2][2];
    return s;
}

inline float4x4 perspective(float fovy, float aspect, float znear, float zfar)
{
    float radians = (fovy / 2.0f) * DegToRad;
    float cotangent = std::cos(radians) / std::sin(radians);
    float z = znear - zfar;

    return{ {
         { cotangent / aspect, 0,         0,                  0                       },
         { 0,                  cotangent, 0,                  0                       },
         { 0,                  0,         (zfar + znear) / z, 2.0f * znear * zfar / z },
         { 0,                  0,         -1,                 0                       },
     } };
}

inline float4x4 orthographic(float left, float right, float bottom, float top, float znear, float zfar)
{
    float x = right - left;
    float y = top - bottom;
    float z = zfar - znear;

    return{ {
         { 2.0f / x, 0,        0,         -(right + left) / x },
         { 0,        2.0f / y, 0,         -(top + bottom) / y },
         { 0,        0,        -2.0f / z, -(zfar + znear) / z },
         { 0,        0,        0,         1                   },
     } };
}

inline float4x4 lookat_rh(const float3& eye, const float3& target, const float3& up)
{
    auto z = normalize(eye - target);
    auto x = normalize(cross(up, z));
    auto y = cross(z, x);

    return { {
        {x.x,          y.x,          z.x,          0},
        {x.y,          y.y,          z.y,          0},
        {x.z,          y.z,          z.z,          0},
        {-dot(x, eye), -dot(y, eye), -dot(z, eye), 1}
    } };
}

inline float4x4 lookat_lh(const float3& eye, const float3& target, const float3& up)
{
    auto z = normalize(target - eye);
    auto x = normalize(cross(up, z));
    auto y = cross(z, x);

    return { {
        {x.x,          y.x,          z.x,          0},
        {x.y,          y.y,          z.y,          0},
        {x.z,          y.z,          z.z,          0},
        {-dot(x, eye), -dot(y, eye), -dot(z, eye), 1}
    } };
}

} // namespace rths
