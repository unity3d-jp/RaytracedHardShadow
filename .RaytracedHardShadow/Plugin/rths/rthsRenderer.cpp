#include "pch.h"
#include "rthsRenderer.h"

namespace rths {

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
    m_camera_buffer = nullptr;
    m_light_buffer = nullptr;
    m_toplevel_as = nullptr;
    m_bottomlevel_as = nullptr;
    m_mesh_buffers.clear();
}

void Renderer::beginScene()
{
    m_mesh_buffers.clear();
}

void Renderer::endScene()
{
}

void Renderer::render()
{
    auto ctx = GfxContext::getInstance();
    ctx->setMeshes(m_mesh_buffers);
    ctx->flush();
}

void Renderer::finish()
{
    auto ctx = GfxContext::getInstance();
    ctx->finish();
}

void Renderer::setRenderTarget(void *rt)
{
    m_render_target = GfxContext::getInstance()->translateTexture(rt);
}

void Renderer::setCamera(const float4x4& trans, float near_, float far_, float fov)
{
}

void Renderer::addDirectionalLight(const float4x4& trans)
{
}

void Renderer::addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_count, int index_offset)
{
    MeshBuffers tmp;
    tmp.vertex_buffer = GfxContext::getInstance()->translateVertexBuffer(vb);
    tmp.index_buffer = GfxContext::getInstance()->translateIndexBuffer(ib);
    if (!tmp.vertex_buffer.resource)
        return;

    tmp.vertex_count = vertex_count;
    tmp.index_count = index_count;
    tmp.index_offset = index_offset;
    m_mesh_buffers.push_back(std::move(tmp));
}

} // namespace rths
