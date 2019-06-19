#include "pch.h"
#include "rthsLog.h"
#include "rthsRenderer.h"
#ifdef _WIN32
#include "rthsGfxContextDXR.h"

namespace rths {

class RendererDXR : public RendererBase
{
public:
    RendererDXR();
    ~RendererDXR() override;

    void render() override;
    void finish() override;

private:
    RenderDataDXR m_render_data;
};


RendererDXR::RendererDXR()
{
    GfxContextDXR::initializeInstance();
}

RendererDXR::~RendererDXR()
{
    GfxContextDXR::finalizeInstance();
}

void RendererDXR::render()
{
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx->validateDevice()) {
        return;
    }
    ctx->prepare(m_render_data);
    ctx->setSceneData(m_render_data, m_scene_data);
    ctx->setRenderTarget(m_render_data, m_render_target);
    ctx->setMeshes(m_render_data, m_mesh_instance_data);
    ctx->flush(m_render_data);
}

void RendererDXR::finish()
{
    auto ctx = GfxContextDXR::getInstance();
    ctx->finish(m_render_data);
    clearMeshInstances();
}

IRenderer* CreateRendererDXR()
{
    return new RendererDXR();
}

} // namespace rths

#else // _WIN32

namespace rths {

IRenderer* CreateRendererDXR()
{
    return nullptr;
}

} // namespace rths

#endif // _WIN32
