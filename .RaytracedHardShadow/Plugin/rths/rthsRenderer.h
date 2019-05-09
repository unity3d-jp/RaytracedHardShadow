#pragma once
#include "rthsGfxContext.h"

namespace rths {

class Renderer
{
public:
    Renderer();
    ~Renderer();

    void beginScene();
    void endScene();
    void render();
    void finish();

    void setRenderTarget(void *rt);
    void setCamera(const float4x4& trans, float near_, float far_, float fov);
    void addDirectionalLight(const float4x4& trans);
    void addMesh(const float4x4& trans, void *vb, void *ib, int vertex_count, int index_count, int index_offset);

private:

    TextureData m_render_target;
    ID3D12ResourcePtr m_camera_buffer;
    ID3D12ResourcePtr m_light_buffer;

    std::vector<MeshBuffers> m_mesh_buffers;
    ID3D12ResourcePtr m_toplevel_as;
    ID3D12ResourcePtr m_bottomlevel_as;
};

} // namespace rths
