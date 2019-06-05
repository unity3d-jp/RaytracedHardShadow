#include "pch.h"
#include "rthsRenderer.h"
#include "rthsLog.h"

namespace rths {

RendererBase::RendererBase()
{
}

RendererBase::~RendererBase()
{
}

void RendererBase::beginScene()
{
    m_scene_data.directional_light_count = 0;
    m_scene_data.spot_light_count = 0;
    m_scene_data.point_light_count = 0;
    m_scene_data.reverse_point_light_count = 0;
    m_mesh_data.clear();
}

void RendererBase::endScene()
{
}

void RendererBase::setRenderTarget(void *rt)
{
    m_render_target.texture = rt;
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
    if (m_scene_data.directional_light_count == kMaxLights) {
        SetErrorLog("exceeded max directional lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.directional_lights[m_scene_data.directional_light_count++];
    dst.direction = extract_direction(trans);
}

void RendererBase::addSpotLight(const float4x4& trans, float range, float spot_angle)
{
    if (m_scene_data.spot_light_count == kMaxLights) {
        SetErrorLog("exceeded max spot lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.spot_lights[m_scene_data.spot_light_count++];
    dst.position = extract_position(trans);
    dst.range = range;
    dst.direction = extract_direction(trans);
    dst.spot_angle = spot_angle * DegToRad;
}

void RendererBase::addPointLight(const float4x4& trans, float range)
{
    if (m_scene_data.point_light_count == kMaxLights) {
        SetErrorLog("exceeded max point lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.point_lights[m_scene_data.point_light_count++];
    dst.position = extract_position(trans);
    dst.range = range;
}

void RendererBase::addReversePointLight(const float4x4& trans, float range)
{
    if (m_scene_data.reverse_point_light_count == kMaxLights) {
        SetErrorLog("exceeded max reverse point lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.reverse_point_lights[m_scene_data.reverse_point_light_count++];
    dst.position = extract_position(trans);
    dst.range = range;
}

void RendererBase::addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_bits, int index_count)
{
    MeshData tmp;
    tmp.vertex_buffer = vb;
    tmp.index_buffer = ib;
    tmp.vertex_count = vertex_count;
    tmp.index_bits = index_bits;
    tmp.index_count = index_count;
    tmp.transform = to_float3x4(trans);
    m_mesh_data.push_back(std::move(tmp));
}

} // namespace rths
