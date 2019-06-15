#include "rthsCommonDeform.h"

struct WeightCount
{
    int weight_count;
    int weight_offset;
};

struct BoneWeight
{
    int bone_index;
    float weight;
};

RWStructuredBuffer<float4>    g_dst_vertices : register(u0); // src & dst
StructuredBuffer<WeightCount> g_weight_counts : register(t1);
StructuredBuffer<BoneWeight>  g_bone_weights : register(t2);
StructuredBuffer<float4x4>    g_bone_matrices : register(t3);
ConstantBuffer<MeshInfo>      g_mesh_info : register(b0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint vid = tid.x;

    float4 base = g_dst_vertices[vid];
    float3 result = float3(0.0f, 0.0f, 0.0f);
    WeightCount wc = g_weight_counts[vid];
    for (int bi = 0; bi < wc.weight_count; ++bi) {
        BoneWeight w = g_bone_weights[wc.weight_offset + bi];
        result += mul(g_bone_matrices[w.bone_index], base).xyz * w.weight;
    }
    g_dst_vertices[vid] = float4(result.xyz, 1.0f);
}
