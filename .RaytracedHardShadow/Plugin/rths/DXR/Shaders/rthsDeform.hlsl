#define kThreadBlockSize 128

enum DeformFlag
{
    DF_APPLY_BLENDSHAPE = 1,
    DF_APPLY_SKINNING = 2,
};

struct BlendshapeFrame
{
    int delta_offset;
    float weight;
};
struct BlendshapeCount
{
    int frame_count;
    int frame_offset;
};

struct BoneCount
{
    int weight_count;
    int weight_offset;
};
struct BoneWeight
{
    float weight;
    int bone_index;
};

struct MeshInfo
{
    int vertex_count;
    int vertex_stride; // in element (e.g. 6 if position + normals)
    int deform_flags;
    int num_blendshapes;
};

RWStructuredBuffer<float4>    g_dst_vertices : register(u0);
StructuredBuffer<float>       g_base_vertices : register(t0);

// blendshape data
StructuredBuffer<float4>            g_bs_delta : register(t1);
StructuredBuffer<BlendshapeFrame>   g_bs_frames : register(t2);
StructuredBuffer<BlendshapeCount>   g_bs_counts : register(t3);
StructuredBuffer<float>             g_bs_weights : register(t4);

// skinning data
StructuredBuffer<BoneCount>   g_bone_counts : register(t5);
StructuredBuffer<BoneWeight>  g_bone_weights : register(t6);
StructuredBuffer<float4x4>    g_bone_matrices : register(t7);

ConstantBuffer<MeshInfo>      g_mesh_info : register(b0);


float3 ApplyBlendshape(uint vid, float3 base)
{
    float3 result = base;
    int num_vertices = g_mesh_info.vertex_count;
    int num_blendshapes = g_mesh_info.num_blendshapes;
    for (int bsi = 0; bsi < num_blendshapes; ++bsi) {
        float weight = g_bs_weights[bsi];
        if (weight != 0.0f) {
            BlendshapeCount count = g_bs_counts[bsi];
            BlendshapeFrame frame = g_bs_frames[count.frame_offset];

            result += g_bs_delta[frame.delta_offset + vid].xyz * weight;
        }
    }
    return result;
}

float3 ApplySkinning(uint vid, float3 base_)
{
    float4 base = float4(base_, 1.0f);
    float3 result = float3(0.0f, 0.0f, 0.0f);
    BoneCount wc = g_bone_counts[vid];
    for (int bi = 0; bi < wc.weight_count; ++bi) {
        BoneWeight w = g_bone_weights[wc.weight_offset + bi];
        result += mul(g_bone_matrices[w.bone_index], base).xyz * w.weight;
    }
    return result;
}

[numthreads(kThreadBlockSize, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint vid = tid.x;

    int vertex_stride = g_mesh_info.vertex_stride;
    float3 result = float3(
        g_base_vertices[vertex_stride * vid + 0],
        g_base_vertices[vertex_stride * vid + 1],
        g_base_vertices[vertex_stride * vid + 2]);

    if ((g_mesh_info.deform_flags & DF_APPLY_BLENDSHAPE) != 0)
        result = ApplyBlendshape(vid, result);
    if ((g_mesh_info.deform_flags & DF_APPLY_SKINNING) != 0)
        result = ApplySkinning(vid, result);

    g_dst_vertices[vid] = float4(result, 1.0f);
}
