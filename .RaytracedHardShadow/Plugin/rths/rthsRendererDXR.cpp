#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsRenderer.h"
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
};

RendererDXR::RendererDXR()
{
}

RendererDXR::~RendererDXR()
{
}

void RendererDXR::render()
{
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx->validateDevice()) {
        return;
    }
    ctx->setSceneData(m_scene_data);
    ctx->setRenderTarget(m_render_target);
    ctx->setMeshes(m_mesh_data);
    ctx->flush();
}

void RendererDXR::finish()
{
    auto ctx = GfxContextDXR::getInstance();
    ctx->finish();
}

IRenderer* CreateRendererDXR()
{
    if (!GfxContextDXR::initializeInstance())
        return nullptr;
    return new RendererDXR();
}

} // namespace rths
#endif
