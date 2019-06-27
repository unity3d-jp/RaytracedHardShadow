#include "pch.h"
#include "rthsRenderer.h"
#include "Foundation/rthsLog.h"

namespace rths {

static std::vector<ISceneCallback*> g_scene_callbacks;

void CallOnMeshDelete(MeshData *mesh)
{
    for (auto& cb : g_scene_callbacks)
        cb->onMeshDelete(mesh);
}

void CallOnMeshInstanceDelete(MeshInstanceData *inst)
{
    for (auto& cb : g_scene_callbacks)
        cb->onMeshInstanceDelete(inst);
}

ISceneCallback::ISceneCallback()
{
    g_scene_callbacks.push_back(this);
}

ISceneCallback::~ISceneCallback()
{
    g_scene_callbacks.erase(std::find(g_scene_callbacks.begin(), g_scene_callbacks.end(), this));
}


static std::vector<RendererBase*> g_renderers;

RendererBase::RendererBase()
{
    g_renderers.push_back(this);
}

RendererBase::~RendererBase()
{
    g_renderers.erase(std::find(g_renderers.begin(), g_renderers.end(), this));
}

void RendererBase::beginScene()
{
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
                // duplicated instance. just merge hit mask.
                prev.hit_mask |= geom.hit_mask;
            }
            else {
                m_geometries_tmp.push_back(prev);
                prev = geom;
            }
        }
        m_geometries_tmp.push_back(prev);
        std::swap(m_geometries, m_geometries_tmp);
    }
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

void RendererBase::setRenderTarget(void *rt)
{
    m_render_target = rt;
}

void RendererBase::setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov)
{
    m_scene_data.camera.view = view;
    m_scene_data.camera.proj = proj;
    m_scene_data.camera.position = extract_position(trans);
    m_scene_data.camera.near_plane = near_;
    m_scene_data.camera.far_plane = far_;
    m_scene_data.camera.fov = fov;
}

void RendererBase::addDirectionalLight(const float4x4& trans)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Directional;
    dst.direction = extract_direction(trans);
}

void RendererBase::addSpotLight(const float4x4& trans, float range, float spot_angle)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Spot;
    dst.position = extract_position(trans);
    dst.range = range;
    dst.direction = extract_direction(trans);
    dst.spot_angle = spot_angle * DegToRad;
}

void RendererBase::addPointLight(const float4x4& trans, float range)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::Point;
    dst.position = extract_position(trans);
    dst.range = range;
}

void RendererBase::addReversePointLight(const float4x4& trans, float range)
{
    if (m_scene_data.light_count == kMaxLights) {
        SetErrorLog("exceeded max lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.lights[m_scene_data.light_count++];
    dst.light_type = LightType::ReversePoint;
    dst.position = extract_position(trans);
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

} // namespace rths


#ifdef _WIN32
    #include "DXR/rthsGfxContextDXR.h"
#endif

namespace rths {

void MarkFrameBegin()
{
    for (auto *cb : g_scene_callbacks)
        cb->frameBegin();
}

void MarkFrameEnd()
{
    for (auto& cb : g_scene_callbacks)
        cb->frameEnd();
}

void RenderAll()
{
    MarkFrameBegin();
    for (auto renderer : g_renderers)
        renderer->render();
    for (auto renderer : g_renderers)
        renderer->finish();
    MarkFrameEnd();
}

} // namespace rths
