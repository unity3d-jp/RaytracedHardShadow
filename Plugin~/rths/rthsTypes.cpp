#include "pch.h"
#include "rthsTypes.h"

namespace rths {

void GlobalSettings::enableDebugFlag(DebugFlag flag)
{
    debug_flags = debug_flags | (uint32_t)flag;
}

void GlobalSettings::disableDebugFlag(DebugFlag flag)
{
    debug_flags = debug_flags & (~(uint32_t)flag);
}

bool GlobalSettings::hasDebugFlag(DebugFlag flag) const
{
    return (debug_flags & (uint32_t)flag) != 0;
}

GlobalSettings& GetGlobals()
{
    static GlobalSettings s_globals;
    return s_globals;
}

static std::vector<std::function<void()>> g_deferred_commands, g_deferred_commands_tmp;
static std::mutex g_mutex_deferred_commands;

template<class Body>
inline void DeferredCommandsLock(const Body& body)
{
    std::unique_lock<std::mutex> l(g_mutex_deferred_commands);
    body();
}

void AddDeferredCommand(const std::function<void()>& v)
{
    DeferredCommandsLock([&v]() {
        g_deferred_commands.push_back(v);
    });
}

void FlushDeferredCommands()
{
    DeferredCommandsLock([]() {
        g_deferred_commands.swap(g_deferred_commands_tmp);
    });
    for (auto& f : g_deferred_commands_tmp)
        f();
    g_deferred_commands_tmp.clear();
}



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

void MeshData::release()
{
    ExternalRelease(this);
}

bool MeshData::valid() const
{
    return this && (
        (gpu_vertex_buffer != nullptr && gpu_index_buffer != nullptr) ||
        (cpu_vertex_buffer != nullptr && cpu_index_buffer != nullptr) );
}

bool MeshData::isRelocated() const
{
    return device_data && device_data->isRelocated();
}


MeshInstanceData::MeshInstanceData()
{
}

MeshInstanceData::~MeshInstanceData()
{
    CallOnMeshInstanceDelete(this);
}

void MeshInstanceData::release()
{
    ExternalRelease(this);
}

bool MeshInstanceData::valid() const
{
    return this && mesh->valid();
}

bool MeshInstanceData::isUpdated(UpdateFlag v) const
{
    return (update_flags & uint32_t(v)) != 0;
}

void MeshInstanceData::clearUpdateFlags()
{
    update_flags = 0;
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

bool MeshInstanceData::hasFlag(InstanceFlag v) const
{
    return (instance_flags & uint32_t(v)) != 0;
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

void MeshInstanceData::setFlags(uint32_t v)
{
    if (instance_flags != v) {
        markUpdated(UpdateFlag::Flags);
        instance_flags = v;
    }
}

void MeshInstanceData::setLayer(uint32_t v)
{
    if (layer != v) {
        markUpdated(UpdateFlag::Flags);
        layer = v;
    }
}



RenderTargetData::RenderTargetData()
{
}

RenderTargetData::~RenderTargetData()
{
    CallOnRenderTargetDelete(this);
}

void RenderTargetData::release()
{
    ExternalRelease(this);
}

bool RenderTargetData::isRelocated() const
{
    return device_data && device_data->isRelocated();
}

} // namespace rths 
