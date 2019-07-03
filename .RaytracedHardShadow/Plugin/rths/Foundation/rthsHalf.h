#pragma once
#include <cstdint>
#include <algorithm>

namespace rths {

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

} // namespace rths
