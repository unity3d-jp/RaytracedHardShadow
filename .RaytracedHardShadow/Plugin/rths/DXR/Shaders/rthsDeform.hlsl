#define kThreadBlockSize 128

enum DEFORM_FLAG
{
    DF_APPLY_BLENDSHAPE = 1,
    DF_APPLY_SKINNING = 2,
};

struct BlendshapeFrame
{
    uint delta_offset;
    float weight;
};
struct BlendshapeInfo
{
    uint frame_count;
    uint frame_offset;
};

struct BoneCount
{
    uint weight_count;
    uint weight_offset;
};
struct BoneWeight
{
    float weight;
    uint bone_index;
};

struct MeshInfo
{
    uint deform_flags; // combination of DEFORM_FLAG
    uint vertex_stride; // in element (e.g. 6 if position + normals)
    uint2 pad;
};

RWStructuredBuffer<float4>    g_dst_vertices : register(u0);
StructuredBuffer<float>       g_base_vertices : register(t0);

// blendshape data
StructuredBuffer<float4>            g_bs_delta : register(t1);
StructuredBuffer<BlendshapeFrame>   g_bs_frames : register(t2);
StructuredBuffer<BlendshapeInfo>    g_bs_info : register(t3);
StructuredBuffer<float>             g_bs_weights : register(t4);

// skinning data
StructuredBuffer<BoneCount>   g_bone_counts : register(t5);
StructuredBuffer<BoneWeight>  g_bone_weights : register(t6);
StructuredBuffer<float4x4>    g_bone_matrices : register(t7);

ConstantBuffer<MeshInfo>      g_mesh_info : register(b0);


uint VertexCount()
{
    uint n, s;
    g_dst_vertices.GetDimensions(n, s);
    return n;
}

uint VertexStrideInElement()
{
    return g_mesh_info.vertex_stride;
}

uint BlendshapeCount()
{
    uint n, s;
    g_bs_info.GetDimensions(n, s);
    return n;
}

uint DeformFlags()
{
    return g_mesh_info.deform_flags;
}


float GetBlendshapeWeight(uint bsi)
{
    return g_bs_weights[bsi];
}

uint GetBlendshapeFrameCount(uint bsi)
{
    return g_bs_info[bsi].frame_count;
}

float GetBlendshapeFrameWeight(uint bsi, uint fi)
{
    uint offset = g_bs_info[bsi].frame_offset;
    return g_bs_frames[offset + fi].weight;
}

float3 GetBlendshapeDelta(uint bsi, uint fi, uint vi)
{
    BlendshapeFrame frame = g_bs_frames[g_bs_info[bsi].frame_offset + fi];
    return g_bs_delta[frame.delta_offset + vi].xyz;
}


uint GetVertexBoneCount(uint vi)
{
    return g_bone_counts[vi].weight_count;
}

float GetVertexBoneWeight(uint vi, uint bi)
{
    return g_bone_weights[g_bone_counts[vi].weight_offset + bi].weight;
}

float4x4 GetVertexBoneMatrix(uint vi, uint bi)
{
    uint i = g_bone_weights[g_bone_counts[vi].weight_offset + bi].bone_index;
    return g_bone_matrices[i];
}


float3 ApplyBlendshape(uint vi, float3 base)
{
    float3 result = base;

    uint blendshape_count = BlendshapeCount();
    for (uint bsi = 0; bsi < blendshape_count; ++bsi) {
        float weight = GetBlendshapeWeight(bsi);
        if (weight == 0.0f)
            continue;

        uint frame_count = GetBlendshapeFrameCount(bsi);
        float last_weight = GetBlendshapeFrameWeight(bsi, frame_count - 1);

        if (weight < 0.0f) {
            float3 delta = GetBlendshapeDelta(bsi, 0, vi);
            float s = weight / GetBlendshapeFrameWeight(bsi, 0);
            result += delta * s;
        }
        else if (weight > last_weight) {
            float3 delta = GetBlendshapeDelta(bsi, frame_count - 1, vi);
            float s = 0.0f;
            if (frame_count >= 2) {
                float prev_weight = GetBlendshapeFrameWeight(bsi, frame_count - 2);
                s = (weight - prev_weight) / (last_weight - prev_weight);
            }
            else {
                s = weight / last_weight;
            }
            result += delta * s;
        }
        else {
            float3 p1 = 0.0f, p2 = 0.0f;
            float w1 = 0.0f, w2 = 0.0f;

            for (uint fi = 0; fi < frame_count; ++fi) {
                float frame_weight = GetBlendshapeFrameWeight(bsi, fi);
                if (weight <= frame_weight) {
                    p2 = GetBlendshapeDelta(bsi, fi, vi);
                    w2 = frame_weight;
                    break;
                }
                else {
                    p1 = GetBlendshapeDelta(bsi, fi, vi);
                    w1 = frame_weight;
                }
            }
            float s = (weight - w1) / (w2 - w1);
            result += lerp(p1, p2, s);
        }
    }
    return result;
}

float3 ApplySkinning(uint vi, float3 base_)
{
    float4 base = float4(base_, 1.0f);
    float3 result = float3(0.0f, 0.0f, 0.0f);

    uint bone_count = GetVertexBoneCount(vi);
    for (uint bi = 0; bi < bone_count; ++bi) {
        float w = GetVertexBoneWeight(vi, bi);
        float4x4 m = GetVertexBoneMatrix(vi, bi);
        result += mul(m, base).xyz * w;
    }
    return result;
}

[numthreads(kThreadBlockSize, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint vi = tid.x;

    uint vertex_stride = VertexStrideInElement();
    float3 result = float3(
        g_base_vertices[vertex_stride * vi + 0],
        g_base_vertices[vertex_stride * vi + 1],
        g_base_vertices[vertex_stride * vi + 2]);

    uint deform_flags = DeformFlags();
    if ((deform_flags & DF_APPLY_BLENDSHAPE) != 0)
        result = ApplyBlendshape(vi, result);
    if ((deform_flags & DF_APPLY_SKINNING) != 0)
        result = ApplySkinning(vi, result);

    g_dst_vertices[vi] = float4(result, 1.0f);
}
