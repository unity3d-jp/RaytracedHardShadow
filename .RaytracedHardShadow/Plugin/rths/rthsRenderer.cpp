#include "pch.h"
#include "rthsRenderer.h"
#include "Foundation/rthsLog.h"

namespace rths {

static std::vector<ISceneCallback*> g_scene_callbacks;
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


static std::vector<IRenderer*> g_renderers;

RendererBase::RendererBase()
    : ref_count(this)
{
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

void RendererBase::beginScene()
{
    m_mutex.lock();
    ++m_update_count;
    m_scene_data.render_flags = 0;
    m_scene_data.light_count = 0;
    m_geometries.clear();
}

void RendererBase::endScene()
{
    if (!m_geometries.empty()) {
        // filter duplicated instances with merging hit masks

        std::sort(m_geometries.begin(), m_geometries.end(),
            [](const auto& a, const auto& b) { return *a.instance < *b.instance; });
        m_geometries_tmp.clear();
        m_geometries_tmp.reserve(m_geometries.size());

        GeometryData prev{ nullptr, 0 };
        for (auto& geom : m_geometries) {
            if (!prev.instance) {
                prev = geom;
            }
            else if (*prev.instance == *geom.instance) {
                // duplicated instance. just merge hit masks.
                prev.receive_mask |= geom.receive_mask;
                prev.cast_mask |= geom.cast_mask;
            }
            else {
                m_geometries_tmp.push_back(prev);
                prev = geom;
            }
        }
        m_geometries_tmp.push_back(prev);
        std::swap(m_geometries, m_geometries_tmp);
    }
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

void RendererBase::setCamera(const float3& pos, const float4x4& view, const float4x4& proj)
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
}

void RendererBase::addDirectionalLight(const float3& dir)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Directional;
    dst.direction = dir;
}

void RendererBase::addSpotLight(const float3& pos, const float3& dir, float range, float spot_angle)
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
}

void RendererBase::addPointLight(const float3& pos, float range)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Point;
    dst.position = pos;
    dst.range = range;
}

void RendererBase::addReversePointLight(const float3& pos, float range)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::ReversePoint;
    dst.position = pos;
    dst.range = range;
}

void RendererBase::addGeometry(GeometryData geom)
{
    if (geom.valid())
        m_geometries.push_back(geom);
}

void RendererBase::clearMeshInstances()
{
    m_geometries.clear();
}


static inline void MarkFrameBeginImpl()
{
    for (auto *cb : g_scene_callbacks)
        cb->frameBegin();
    for (auto renderer : g_renderers)
        renderer->frameBegin();
}

static inline void MarkFrameEndImpl()
{
    for (auto renderer : g_renderers)
        renderer->frameEnd();
    for (auto& cb : g_scene_callbacks)
        cb->frameEnd();
}

void MarkFrameBegin()
{
    SceneCallbacksLock([]() {
        MarkFrameBeginImpl();
    });
}

void MarkFrameEnd()
{
    SceneCallbacksLock([]() {
        MarkFrameEndImpl();
    });
}

void RenderAll()
{
    SceneCallbacksLock([]() {
        MarkFrameBeginImpl();
        for (auto renderer : g_renderers)
            renderer->render();
        for (auto renderer : g_renderers)
            renderer->finish();
        MarkFrameEndImpl();
    });
}

} // namespace rths
