#include "pch.h"
#ifdef _WIN32
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
    void setCamera(const float4x4& trans, float near_, float far_, float fov) override;
    void addDirectionalLight(const float4x4& trans) override;
    void addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_count, int index_offset) override;

private:
    TextureDataDXR m_render_target;
    ID3D12ResourcePtr m_camera_buffer;
    ID3D12ResourcePtr m_light_buffer;
    std::vector<MeshBuffersDXR> m_mesh_buffers;
};

RendererDXR::RendererDXR()
{
}

RendererDXR::~RendererDXR()
{
    m_camera_buffer = nullptr;
    m_light_buffer = nullptr;
    m_mesh_buffers.clear();
}

void RendererDXR::beginScene()
{
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

void RendererDXR::setCamera(const float4x4& trans, float near_, float far_, float fov)
{
    // todo
}

void RendererDXR::addDirectionalLight(const float4x4& trans)
{
    // todo
}

void RendererDXR::addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_count, int index_offset)
{
    MeshBuffersDXR tmp;
    tmp.vertex_buffer.buffer = vb;
    tmp.index_buffer.buffer = ib;
    tmp.vertex_count = vertex_count;
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
