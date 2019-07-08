#include "pch.h"
#include "rthsTypes.h"

namespace rths {

bool SkinData::valid() const
{
    return !bindposes.empty() && !bone_counts.empty() && !weights.empty();
}

void CallOnMeshDelete(MeshData *mesh);
void CallOnMeshInstanceDelete(MeshInstanceData *inst);
void CallOnRenderTargetDelete(RenderTargetData *rt);


MeshData::MeshData()
{
}

MeshData::~MeshData()
{
    CallOnMeshDelete(this);
}

bool MeshData::valid() const
{
    return
        (gpu_vertex_buffer != nullptr && gpu_index_buffer != nullptr) ||
        (cpu_vertex_buffer != nullptr && cpu_index_buffer != nullptr);
}



MeshInstanceData::MeshInstanceData()
{
}

MeshInstanceData::~MeshInstanceData()
{
    CallOnMeshInstanceDelete(this);
}

bool MeshInstanceData::valid() const
{
    return mesh && mesh->valid();
}

bool MeshInstanceData::isUpdated(UpdateFlag v) const
{
    return (update_flags & uint32_t(v)) != 0;
}

void MeshInstanceData::markUpdated(UpdateFlag v)
{
    update_flags |= (uint32_t)v;
}

void MeshInstanceData::markUpdated()
{
    if (mesh) {
        markUpdated(UpdateFlag::Transform);
        if (mesh->skin.valid())
            markUpdated(UpdateFlag::Bones);
        if (!mesh->blendshapes.empty())
            markUpdated(UpdateFlag::Blendshape);
    }
}

void MeshInstanceData::setTransform(const float4x4 &v)
{
    if (transform != v) {
        transform = v;
        markUpdated(UpdateFlag::Transform);
    }
}

void MeshInstanceData::setBones(const float4x4 *v, size_t n)
{
    if (bones.size() != n)
        markUpdated(UpdateFlag::Bones);

    if (n == 0) {
        bones.clear();
    }
    else {
        if (bones.size() == n && !std::equal(v, v + n, bones.data()))
            markUpdated(UpdateFlag::Bones);
        bones.assign(v, v + n);
    }
}

void MeshInstanceData::setBlendshapeWeights(const float *v, size_t n)
{
    if (blendshape_weights.size() != n)
        markUpdated(UpdateFlag::Blendshape);

    if (n == 0) {
        blendshape_weights.clear();
    }
    else {
        if (blendshape_weights.size() == n && !std::equal(v, v + n, blendshape_weights.data()))
            markUpdated(UpdateFlag::Blendshape);
        blendshape_weights.assign(v, v + n);
    }
}

bool GeometryData::valid() const
{
    return (receive_mask | cast_mask) != 0 && instance && instance->valid();
}

void GeometryData::markUpdated()
{
    if (instance)
        instance->markUpdated();
}



RenderTargetData::RenderTargetData()
{
}

RenderTargetData::~RenderTargetData()
{
    CallOnRenderTargetDelete(this);
}

} // namespace rths 
