#pragma once
#include "rthsTypes.h"

namespace rths {

class ISceneCallback
{
public:
    ISceneCallback();
    virtual ~ISceneCallback();

    virtual void frameBegin() = 0;
    virtual void frameEnd() = 0;

    virtual void onMeshDelete(MeshData *mesh) = 0;
    virtual void onMeshInstanceDelete(MeshInstanceData *inst) = 0;
    virtual void onRenderTargetDelete(RenderTargetData *rt) = 0;
};

class IRenderer
{
public:
    virtual ~IRenderer() {}
    virtual void release() = 0;
    virtual bool initialized() const = 0;
    virtual bool valid() const = 0;

    virtual void beginScene() = 0;
    virtual void endScene() = 0;

    virtual void setName(const std::string& name) = 0;
    virtual void setRaytraceFlags(uint32_t flags) = 0;
    virtual void setShadowRayOffset(float v) = 0;
    virtual void setSelfShadowThreshold(float v) = 0;

    virtual void setRenderTarget(RenderTargetData *rt) = 0;
    virtual void setCamera(const float3& pos, const float4x4& view, const float4x4& proj, uint32_t lmask) = 0;
    virtual void addDirectionalLight(const float3& dir, uint32_t lmask) = 0;
    virtual void addSpotLight(const float3& pos, const float3& dir, float range, float spot_angle, uint32_t lmask) = 0;
    virtual void addPointLight(const float3& pos, float range, uint32_t lmask) = 0;
    virtual void addReversePointLight(const float3& pos, float range, uint32_t lmask) = 0;
    virtual void addMesh(MeshInstanceDataPtr mesh) = 0;

    virtual bool isRendering() const = 0;
    virtual void frameBegin() = 0; // called from render thread
    virtual void render() = 0; // called from render thread
    virtual void finish() = 0; // called from render thread
    virtual void frameEnd() = 0; // called from render thread

    virtual bool readbackRenderTarget(void *dst) = 0;
    virtual std::string getTimestampLog() = 0;
    virtual void* getRenderTexturePtr() = 0;
};


class RendererBase : public IRenderer, public SharedResource<RendererBase>
{
using ref_count = SharedResource<RendererBase>;
public:
    RendererBase();
    ~RendererBase() override;
    void release() override;

    void beginScene() override;
    void endScene() override;

    void setRaytraceFlags(uint32_t flags) override;
    void setShadowRayOffset(float v) override;
    void setSelfShadowThreshold(float v) override;

    void setRenderTarget(RenderTargetData *rt) override;
    void setCamera(const float3& pos, const float4x4& view, const float4x4& proj, uint32_t lmask) override;
    void addDirectionalLight(const float3& dir, uint32_t lmask) override;
    void addSpotLight(const float3& pos, const float3& dir, float range, float spot_angle, uint32_t lmask) override;
    void addPointLight(const float3& dir, float range, uint32_t lmask) override;
    void addReversePointLight(const float3& dir, float range, uint32_t lmask) override;
    void addMesh(MeshInstanceDataPtr mesh) override;

protected:
    SceneData m_scene_data;
    RenderTargetDataPtr m_render_target;
    std::mutex m_mutex;
    mutable std::atomic_bool m_is_updating{ false };
    mutable std::atomic_bool m_is_rendering{ false };
    std::atomic_int m_update_count{ 0 };
    std::atomic_int m_render_count{ 0 };
    std::atomic_int m_skip_count{ 0 };

    std::vector<MeshInstanceDataPtr> m_meshes;
    std::array<std::vector<MeshInstanceDataPtr>, rthsMaxLayers> m_layers;
    std::array<uint32_t, rthsMaxLayers> m_layer_lut;
};

IRenderer* CreateRendererDXR();

void MarkFrameBegin();
void MarkFrameEnd();
void RenderAll();

} // namespace rths
