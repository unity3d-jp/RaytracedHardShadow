#include "rthsCommonDeform.h"

RWStructuredBuffer<float4>    g_dst_vertices : register(u0);
StructuredBuffer<float>       g_src_vertices : register(t0);
ConstantBuffer<MeshInfo>      g_mesh_info : register(b0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint vid = tid.x;

    int vertex_stride = g_mesh_info.vertex_stride;
    g_dst_vertices[vid] = float4(
        g_src_vertices[vertex_stride * vid + 0],
        g_src_vertices[vertex_stride * vid + 1],
        g_src_vertices[vertex_stride * vid + 2],
        1.0f);
}
