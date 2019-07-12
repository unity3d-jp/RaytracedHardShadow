#pragma once
#include <cstdint>
#include <algorithm>

namespace rths {

inline float clamp01(float v);

// note: this half doesn't care about Inf nor NaN. simply round down minor bits of exponent and mantissa.
struct half
{
    uint16_t value;

    half() {}
    half(const half& v) : value(v.value) {}

    half(float v)
    {
        uint32_t n = (uint32_t&)v;
        uint16_t sign_bit = (n >> 16) & 0x8000;
        uint16_t exponent = (std::max<int>((n >> 23) - 127 + 15, 0) & 0x1f) << 10;
        uint16_t mantissa = (n >> (23 - 10)) & 0x3ff;

        value = sign_bit | exponent | mantissa;
    }

    half& operator=(float v)
    {
        *this = half(v);
        return *this;
    }

    operator float() const
    {
        uint32_t sign_bit = (value & 0x8000) << 16;
        uint32_t exponent = ((((value >> 10) & 0x1f) - 15 + 127) & 0xff) << 23;
        uint32_t mantissa = (value & 0x3ff) << (23 - 10);

        uint32_t r = sign_bit | exponent | mantissa;
        return (float&)r;
    }

    static half zero() { return half(0.0f); }
    static half one() { return half(1.0f); }
};

// 0.0f - 1.0f <-> 0 - 255
struct unorm8
{
    static constexpr float C = float(0xff);
    static constexpr float R = 1.0f / float(0xff);

    uint8_t value;

    unorm8() {}
    unorm8(const unorm8& v) : value(v.value) {}
    unorm8(float v) : value(uint8_t(clamp01(v) * C)) {}

    unorm8& operator=(float v)
    {
        *this = unorm8(v);
        return *this;
    }
    operator float() const { return (float)value * R; }

    static unorm8 zero() { return unorm8(0.0f); }
    static unorm8 one() { return unorm8(1.0f); }
};

} // namespace rths
