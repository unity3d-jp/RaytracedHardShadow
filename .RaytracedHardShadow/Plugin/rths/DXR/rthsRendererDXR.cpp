#include "pch.h"
#include "Foundation/rthsLog.h"
#include "rthsRenderer.h"
#ifdef _WIN32
#include "rthsGfxContextDXR.h"

namespace rths {

class RendererDXR : public RendererBase
{
public:
    RendererDXR();
    ~RendererDXR() override;

    bool valid() const override;
    void render() override; // called from render thread
    void finish() override; // called from render thread

    bool readbackRenderTarget(void *dst) override;

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

bool RendererDXR::valid() const
{
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx || !ctx->validateDevice())
        return false;
    return true;
}

void RendererDXR::render()
{
    if (!valid())
        return;

    auto ctx = GfxContextDXR::getInstance();
    ctx->prepare(m_render_data);
    ctx->setSceneData(m_render_data, m_scene_data);
    ctx->setRenderTarget(m_render_data, m_render_target);
    ctx->setGeometries(m_render_data, m_geometries);
    ctx->flush(m_render_data);
}

void RendererDXR::finish()
{
    if (!valid())
        return;

    auto ctx = GfxContextDXR::getInstance();
    ctx->finish(m_render_data);
    clearMeshInstances();
}

bool RendererDXR::readbackRenderTarget(void *dst)
{
    if (!valid())
        return false;

    auto ctx = GfxContextDXR::getInstance();
    return ctx->readbackRenderTarget(m_render_data, dst);
}

IRenderer* CreateRendererDXR()
{
    auto ret = new RendererDXR();
    if (!ret->valid()) {
        delete ret;
        ret = nullptr;
    }
    return ret;
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
