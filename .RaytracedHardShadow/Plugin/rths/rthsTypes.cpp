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

bool GeometryData::valid() const
{
    return (receive_mask | cast_mask) != 0 && instance && instance->valid();
}



RenderTargetData::RenderTargetData()
{
}

RenderTargetData::~RenderTargetData()
{
    CallOnRenderTargetDelete(this);
}

} // namespace rths 
