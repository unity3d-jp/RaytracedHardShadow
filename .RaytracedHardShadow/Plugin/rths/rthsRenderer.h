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
    virtual bool valid() const = 0;

    virtual void beginScene() = 0;
    virtual void endScene() = 0;

    virtual void setRaytraceFlags(uint32_t flags) = 0;
    virtual void setShadowRayOffset(float v) = 0;
    virtual void setSelfShadowThreshold(float v) = 0;

    virtual void setRenderTarget(RenderTargetData *rt) = 0;
    virtual void setCamera(const float3& pos, const float4x4& view, const float4x4& proj) = 0;
    virtual void addDirectionalLight(const float3& dir) = 0;
    virtual void addSpotLight(const float3& pos, const float3& dir, float range, float spot_angle) = 0;
    virtual void addPointLight(const float3& pos, float range) = 0;
    virtual void addReversePointLight(const float3& pos, float range) = 0;
    virtual void addGeometry(GeometryData geom) = 0;

    virtual void frameBegin() = 0; // called from render thread
    virtual void frameEnd() = 0; // called from render thread
    virtual void render() = 0; // called from render thread
    virtual void finish() = 0; // called from render thread

    virtual bool readbackRenderTarget(void *dst) = 0;
    virtual const char* getTimestampLog() = 0;
    virtual void* getRenderTexturePtr() = 0;
};


class RendererBase : public IRenderer
{
public:
    RendererBase();
    ~RendererBase() override;

    void beginScene() override;
    void endScene() override;

    void setRaytraceFlags(uint32_t flags) override;
    void setShadowRayOffset(float v) override;
    void setSelfShadowThreshold(float v) override;

    void setRenderTarget(RenderTargetData *rt) override;
    void setCamera(const float3& pos, const float4x4& view, const float4x4& proj) override;
    void addDirectionalLight(const float3& dir) override;
    void addSpotLight(const float3& pos, const float3& dir, float range, float spot_angle) override;
    void addPointLight(const float3& dir, float range) override;
    void addReversePointLight(const float3& dir, float range) override;
    void addGeometry(GeometryData geom) override;

protected:
    void clearMeshInstances();

    SceneData m_scene_data;
    RenderTargetDataPtr m_render_target;
    std::vector<GeometryData> m_geometries;

private:
    std::vector<GeometryData> m_geometries_tmp;
};

IRenderer* CreateRendererDXR();

void MarkFrameBegin();
void MarkFrameEnd();
void RenderAll();

} // namespace rths
