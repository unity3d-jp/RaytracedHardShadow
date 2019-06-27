#include "pch.h"
#include "rthsTypes.h"

namespace rths {

bool SkinData::valid() const
{
    return !bindposes.empty() && !bone_counts.empty() && !weights.empty();
}

void CallOnMeshDelete(MeshData *mesh);
void CallOnMeshInstanceDelete(MeshInstanceData *inst);

static uint64_t g_meshdata_id = 0;

MeshData::MeshData()
{
    id = ++g_meshdata_id;
}

MeshData::~MeshData()
{
    CallOnMeshDelete(this);
}

void MeshData::addref()
{
    ++ref_count;
}

void MeshData::release()
{
    if (--ref_count == 0)
        delete this;
}

bool MeshData::valid() const
{
    return
        (gpu_vertex_buffer != nullptr && gpu_index_buffer != nullptr) ||
        (cpu_vertex_buffer != nullptr && cpu_index_buffer != nullptr);
}

bool MeshData::operator==(const MeshData & v) const
{
    return id == v.id;
}
bool MeshData::operator!=(const MeshData & v) const
{
    return id != v.id;
}
bool MeshData::operator<(const MeshData& v) const
{
    return id < v.id;
}


static uint64_t g_meshinstancedata_id = 0;

MeshInstanceData::MeshInstanceData()
{
    id = ++g_meshinstancedata_id;
}

MeshInstanceData::~MeshInstanceData()
{
    CallOnMeshInstanceDelete(this);
}

void MeshInstanceData::addref()
{
    ++ref_count;
}

void MeshInstanceData::release()
{
    if (--ref_count == 0)
        delete this;
}

bool MeshInstanceData::valid() const
{
    return mesh && mesh->valid();
}

bool MeshInstanceData::operator==(const MeshInstanceData & v) const
{
    return id == v.id;
}
bool MeshInstanceData::operator!=(const MeshInstanceData & v) const
{
    return id != v.id;
}
bool MeshInstanceData::operator<(const MeshInstanceData & v) const
{
    return id < v.id;
}

bool GeometryData::valid() const
{
    return hit_mask != 0 && instance && instance->valid();
}

} // namespace rths 
