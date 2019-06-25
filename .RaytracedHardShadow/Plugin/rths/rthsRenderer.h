#pragma once
#include "rthsTypes.h"

namespace rths {

using FrameBeginCallback = std::function<void()>;
using FrameEndCallback = std::function<void()>;

class ISceneCallback
{
public:
    ISceneCallback();
    virtual ~ISceneCallback();

    virtual void frameBegin() = 0;
    virtual void frameEnd() = 0;

    virtual void onMeshDelete(MeshData *mesh) = 0;
    virtual void onMeshInstanceDelete(MeshInstanceData *inst) = 0;
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

    virtual void setRenderTarget(void *rt) = 0;
    virtual void setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov) = 0;
    virtual void addDirectionalLight(const float4x4& trans) = 0;
    virtual void addSpotLight(const float4x4& trans, float range, float spot_angle) = 0;
    virtual void addPointLight(const float4x4& trans, float range) = 0;
    virtual void addReversePointLight(const float4x4& trans, float range) = 0;
    virtual void addGeometry(GeometryData geom) = 0;

    virtual void render() = 0; // called from render thread
    virtual void finish() = 0; // called from render thread
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

    void setRenderTarget(void *rt) override;
    void setCamera(const float4x4& trans, const float4x4& view, const float4x4& proj, float near_, float far_, float fov) override;
    void addDirectionalLight(const float4x4& trans) override;
    void addSpotLight(const float4x4& trans, float range, float spot_angle) override;
    void addPointLight(const float4x4& trans, float range) override;
    void addReversePointLight(const float4x4& trans, float range) override;
    void addGeometry(GeometryData geom) override;

protected:
    void clearMeshInstances();

    TextureData m_render_target;
    SceneData m_scene_data;
    std::vector<GeometryData> m_geometries;

private:
    std::vector<GeometryData> m_geometries_tmp;
};

IRenderer* CreateRendererDXR();

void MarkFrameBegin();
void MarkFrameEnd();
void RenderAll();

} // namespace rths
