#pragma once
#include "rthsTypes.h"

namespace rths {

class IRenderer
{
public:
    virtual ~IRenderer() {}

    virtual void beginScene() = 0;
    virtual void endScene() = 0;
    virtual void render() = 0;
    virtual void finish() = 0;

    virtual void setRaytraceFlags(int flags) = 0;
    virtual void setShadowRayOffset(float v) = 0;
    virtual void setSelfShadowThreshold(float v) = 0;

    virtual void setRenderTarget(void *rt) = 0;
    virtual void setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov) = 0;
    virtual void addDirectionalLight(const float4x4& trans) = 0;
    virtual void addSpotLight(const float4x4& trans, float range, float spot_angle) = 0;
    virtual void addPointLight(const float4x4& trans, float range) = 0;
    virtual void addReversePointLight(const float4x4& trans, float range) = 0;
    virtual void addMesh(const float4x4& trans, void *vb, void *ib,
        int vertex_count, int index_bits, int index_count, int index_offset, bool is_dynamic) = 0;
};


class RendererBase : public IRenderer
{
public:
    RendererBase();
    ~RendererBase() override;

    void beginScene() override;
    void endScene() override;

    void setRaytraceFlags(int flags) override;
    void setShadowRayOffset(float v) override;
    void setSelfShadowThreshold(float v) override;

    void setRenderTarget(void *rt) override;
    void setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov) override;
    void addDirectionalLight(const float4x4& trans) override;
    void addSpotLight(const float4x4& trans, float range, float spot_angle) override;
    void addPointLight(const float4x4& trans, float range) override;
    void addReversePointLight(const float4x4& trans, float range) override;
    void addMesh(const float4x4& trans, void *vb, void *ib,
        int vertex_count, int index_bits, int index_count, int index_offset, bool is_dynamic) override;

protected:
    TextureData m_render_target;
    SceneData m_scene_data;
    std::vector<MeshData> m_mesh_data;
};

IRenderer* CreateRendererDXR();
void RenderAll();

} // namespace rths
