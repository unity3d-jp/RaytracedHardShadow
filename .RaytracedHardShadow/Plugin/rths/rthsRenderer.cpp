#include "pch.h"
#include "rthsRenderer.h"
#include "rthsLog.h"

namespace rths {

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
    m_mesh_instance_data.clear();
}

void RendererBase::endScene()
{
}

void RendererBase::setRaytraceFlags(int flags)
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

void RendererBase::addMesh(MeshInstanceData *mesh)
{
    m_mesh_instance_data.push_back(mesh);
}

void RendererBase::clearMeshInstances()
{
    m_mesh_instance_data.clear();
}

} // namespace rths


#ifdef _WIN32
    #include "DXR/rthsGfxContextDXR.h"
#endif

namespace rths {

void RenderAll()
{
    for (auto renderer : g_renderers) {
        renderer->render();
        renderer->finish();
    }

#ifdef _WIN32
    if(auto ctx = GfxContextDXR::getInstance())
        ctx->releaseUnusedResources();
#endif
}

} // namespace rths
