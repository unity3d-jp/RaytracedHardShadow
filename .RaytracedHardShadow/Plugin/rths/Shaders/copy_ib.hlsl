#include "common.h"

RWBuffer<int> _Input : register(u0);
RWBuffer<int> _Output : register(u1);


[numthreads(32, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint ti = tid.x;
    _Output[ti] = _Input[ti];
}
