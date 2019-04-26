#include "pch.h"
#include "rthsRenderer.h"

namespace rths {

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
    m_render_target = nullptr;
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

void Renderer::addMesh(const float4x4& trans, void *vb, void *ib)
{
    MeshBuffers tmp;
    tmp.m_vertex_buffer = GfxContext::getInstance()->translateVertexBuffer(vb);
    tmp.m_index_buffer = GfxContext::getInstance()->translateIndexBuffer(ib);
    if (tmp.m_vertex_buffer && tmp.m_index_buffer)
        m_mesh_buffers.push_back(std::move(tmp));
}

} // namespace rths
