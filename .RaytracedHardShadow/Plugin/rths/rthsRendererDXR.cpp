#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsRenderer.h"
#include "rthsGfxContextDXR.h"

namespace rths {
    
class RendererDXR : public IRenderer
{
public:
    RendererDXR();
    ~RendererDXR() override;

    void beginScene() override;
    void endScene() override;
    void render() override;
    void finish() override;

    void setRenderTarget(void *rt) override;
    void setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov) override;
    void addDirectionalLight(const float4x4& trans) override;
    void addPointLight(const float4x4& trans) override;
    void addReversePointLight(const float4x4& trans) override;
    void addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_bits, int index_count, int index_offset) override;

private:
    TextureDataDXR m_render_target;
    SceneData m_scene_data;
    std::vector<MeshBuffersDXR> m_mesh_buffers;
};

RendererDXR::RendererDXR()
{
}

RendererDXR::~RendererDXR()
{
    m_mesh_buffers.clear();
}

void RendererDXR::beginScene()
{
    m_scene_data.directional_light_count = 0;
    m_scene_data.point_light_count = 0;
    m_scene_data.reverse_point_light_count = 0;
    m_mesh_buffers.clear();
}

void RendererDXR::endScene()
{
}

void RendererDXR::render()
{
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx->validateDevice()) {
        return;
    }
    ctx->setSceneData(m_scene_data);
    ctx->setRenderTarget(m_render_target);
    ctx->setMeshes(m_mesh_buffers);
    ctx->flush();
}

void RendererDXR::finish()
{
    auto ctx = GfxContextDXR::getInstance();
    ctx->finish();
}

void RendererDXR::setRenderTarget(void *rt)
{
    m_render_target.texture = rt;
}

void RendererDXR::setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov)
{
    m_scene_data.camera.view = view;
    m_scene_data.camera.proj = proj;
    m_scene_data.camera.position = extract_position(trans);
    m_scene_data.camera.near_plane = near_;
    m_scene_data.camera.far_plane = far_;
    m_scene_data.camera.fov = fov;
}

void RendererDXR::addDirectionalLight(const float4x4& trans)
{
    if (m_scene_data.directional_light_count == kMaxLights) {
        SetErrorLog("exceeded max directional lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.directional_lights[m_scene_data.directional_light_count++];
    dst.direction = extract_direction(trans);
}

void RendererDXR::addPointLight(const float4x4& trans)
{
    if (m_scene_data.point_light_count == kMaxLights) {
        SetErrorLog("exceeded max point lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.point_lights[m_scene_data.point_light_count++];
    dst.position = extract_position(trans);
}

void RendererDXR::addReversePointLight(const float4x4& trans)
{
    if (m_scene_data.reverse_point_light_count == kMaxLights) {
        SetErrorLog("exceeded max reverse point lights (%d)\n", kMaxLights);
        return;
    }
    auto& dst = m_scene_data.reverse_point_lights[m_scene_data.reverse_point_light_count++];
    dst.position = extract_position(trans);
}

void RendererDXR::addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_bits, int index_count, int index_offset)
{
    MeshBuffersDXR tmp;
    tmp.vertex_buffer.buffer = vb;
    tmp.index_buffer.buffer = ib;
    tmp.vertex_count = vertex_count;
    tmp.index_bits = index_bits;
    tmp.index_count = index_count;
    tmp.index_offset = index_offset;
    tmp.transform = to_float3x4(trans);
    m_mesh_buffers.push_back(std::move(tmp));
}

IRenderer* CreateRendererDXR()
{
    if (!GfxContextDXR::initializeInstance())
        return nullptr;
    return new RendererDXR();
}

} // namespace rths
#endif
