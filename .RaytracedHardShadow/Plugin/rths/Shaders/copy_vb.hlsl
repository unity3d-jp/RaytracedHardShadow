#include "common.h"

RWBuffer<float> _Input : register(u0);
RWBuffer<float> _Output : register(u1);

[numthreads(32, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint ti = tid.x;
    for (int i = 0; i < 3; ++i)
        _Output[ti * 3 + i] = _Input[ti * stride_vertices + i];
}
