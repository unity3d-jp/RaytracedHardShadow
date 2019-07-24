#include "pch.h"
#include "rthsRenderer.h"
#include "Foundation/rthsLog.h"

namespace rths {

static std::vector<ISceneCallback*> g_scene_callbacks, g_scene_callbacks_tmp;
static std::vector<IRenderer*> g_renderers, g_renderers_tmp;
static std::mutex g_mutex_scene_callbacks;

template<class Body>
inline void SceneCallbacksLock(const Body& body)
{
    std::unique_lock<std::mutex> l(g_mutex_scene_callbacks);
    body();
}

void CallOnMeshDelete(MeshData *mesh)
{
    SceneCallbacksLock([mesh]() {
        for (auto& cb : g_scene_callbacks)
            cb->onMeshDelete(mesh);
    });
}

void CallOnMeshInstanceDelete(MeshInstanceData *inst)
{
    SceneCallbacksLock([inst]() {
        for (auto& cb : g_scene_callbacks)
            cb->onMeshInstanceDelete(inst);
    });
}

void CallOnRenderTargetDelete(RenderTargetData *rt)
{
    SceneCallbacksLock([rt]() {
        for (auto& cb : g_scene_callbacks)
            cb->onRenderTargetDelete(rt);
    });
}

ISceneCallback::ISceneCallback()
{
    SceneCallbacksLock([this]() {
        g_scene_callbacks.push_back(this);
    });
}

ISceneCallback::~ISceneCallback()
{
    SceneCallbacksLock([this]() {
        g_scene_callbacks.erase(std::find(g_scene_callbacks.begin(), g_scene_callbacks.end(), this));
    });
}


static int g_renderer_id_seed = 0;

IRenderer* FindRendererByID(int id)
{
    // linear search. but I believe this is acceptable as renderers won't be so many at the same time.
    for (auto r : g_renderers)
        if (r->getID() == id)
            return r;
    return nullptr;
}


RendererBase::RendererBase()
    : ref_count(this)
{
    m_id = ++g_renderer_id_seed;

    SceneCallbacksLock([this]() {
        g_renderers.push_back(this);
    });
}

RendererBase::~RendererBase()
{
    SceneCallbacksLock([this]() {
        g_renderers.erase(std::find(g_renderers.begin(), g_renderers.end(), this));
    });
}

void RendererBase::release()
{
    ExternalRelease(this);
}

int RendererBase::getID() const
{
    return m_id;
}

void RendererBase::beginScene()
{
    m_mutex.lock();
    m_is_updating = true;

    m_scene_data.render_flags = 0;
    m_scene_data.light_count = 0;

    m_meshes.clear();
}

void RendererBase::endScene()
{
    if (m_render_target)
        m_scene_data.output_format = (uint32_t)m_render_target->output_format;

    // setup object layer mask
    for (auto& inst : m_meshes)
        inst->layer_mask = 0x1 << inst->layer;

    m_is_updating = false;
    m_ready_to_render = true;
    m_mutex.unlock();
}

void RendererBase::setRaytraceFlags(uint32_t flags)
{
    m_scene_data.render_flags = flags;
}

void RendererBase::setShadowRayOffset(float v)
{
    m_scene_data.shadow_ray_offset = v;
}

void RendererBase::setSelfShadowThreshold(float v)
{
    m_scene_data.self_shadow_threshold = v;
}

void RendererBase::setRenderTarget(RenderTargetData *rt)
{
    m_render_target = rt;
}

void RendererBase::setCamera(const float3& pos, const float4x4& view, const float4x4& proj, uint32_t lmask)
{
    m_scene_data.camera.view = view;
    m_scene_data.camera.proj = proj;
    m_scene_data.camera.position = pos;
    {
        auto m22 = -proj[2][2];
        auto m32 = -proj[3][2];
        auto tmp_near = std::abs((2.0f * m32) / (2.0f*m22 - 2.0f));
        auto tmp_far = std::abs(((m22 - 1.0f)*tmp_near) / (m22 + 1.0f));
        if (tmp_near > tmp_far)
            std::swap(tmp_near, tmp_far);
        m_scene_data.camera.near_plane = tmp_near;
        m_scene_data.camera.far_plane = tmp_far;
    }
    m_scene_data.camera.layer_mask = lmask;
}

void RendererBase::addDirectionalLight(const float3& dir, uint32_t lmask)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Directional;
    dst.direction = dir;
    dst.layer_mask = lmask;
}

void RendererBase::addSpotLight(const float3& pos, const float3& dir, float range, float spot_angle, uint32_t lmask)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Spot;
    dst.position = pos;
    dst.range = range;
    dst.direction = dir;
    dst.spot_angle = spot_angle * DegToRad;
    dst.layer_mask = lmask;
}

void RendererBase::addPointLight(const float3& pos, float range, uint32_t lmask)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Point;
    dst.position = pos;
    dst.range = range;
    dst.layer_mask = lmask;
}

void RendererBase::addReversePointLight(const float3& pos, float range, uint32_t lmask)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::ReversePoint;
    dst.position = pos;
    dst.range = range;
    dst.layer_mask = lmask;
}

void RendererBase::addMesh(MeshInstanceDataPtr mesh)
{
    if (mesh->valid())
        m_meshes.push_back(mesh);
}


void MarkFrameBegin()
{
    SceneCallbacksLock([]() {
        g_scene_callbacks_tmp = g_scene_callbacks;
        g_renderers_tmp = g_renderers;
    });
    for (auto *cb : g_scene_callbacks_tmp)
        cb->frameBegin();
    for (auto renderer : g_renderers_tmp)
        renderer->frameBegin();
}

void MarkFrameEnd()
{
    for (auto renderer : g_renderers_tmp)
        renderer->frameEnd();
    for (auto& cb : g_scene_callbacks_tmp)
        cb->frameEnd();
}

void RenderAll()
{
    MarkFrameBegin();
    for (auto renderer : g_renderers_tmp)
        renderer->render();
    for (auto renderer : g_renderers_tmp)
        renderer->finish();
    MarkFrameEnd();
}

} // namespace rths
