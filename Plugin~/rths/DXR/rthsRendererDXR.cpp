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
    void setName(const std::string& name) override;

    bool initialized() const override;
    bool valid() const override;

    bool isRendering() const override;
    void frameBegin() override; // called from render thread
    void render() override; // called from render thread
    void finish() override; // called from render thread
    void frameEnd() override; // called from render thread

    bool readbackRenderTarget(void *dst) override;
    std::string getTimestampLog() override;
    void* getRenderTexturePtr() override;

private:
    RenderDataDXR m_render_data;
    std::atomic_bool m_is_initialized{ false };
};


RendererDXR::RendererDXR()
{
    auto do_init = [this]() {
        GfxContextDXR::initializeInstance();
        m_is_initialized = true;
    };

    if (GetGlobals().deferred_initilization)
        AddDeferredCommand(do_init);
    else
        do_init();
}

RendererDXR::~RendererDXR()
{
    if (m_is_initialized) {
        GfxContextDXR::finalizeInstance();
    }
}

void RendererDXR::setName(const std::string& name)
{
    m_render_data.name = name;
}

bool RendererDXR::initialized() const
{
    return m_is_initialized;
}

bool RendererDXR::valid() const
{
    auto ctx = GfxContextDXR::getInstance();
    if (!ctx || !ctx->checkError()) {
        m_is_rendering = false;
        return false;
    }
    return true;
}

bool RendererDXR::isRendering() const
{
    return m_is_rendering;
}

void RendererDXR::frameBegin()
{
    if (m_render_data.hasFlag(RenderFlag::DbgForceUpdateAS)) {
        // clear static meshes' BLAS
        for (auto& geom : m_render_data.geometries_prev)
            geom.clearBLAS();

        // mark updated to update deformable meshes' BLAS
        for (auto& geom : m_render_data.geometries_prev)
            geom.inst->base->markUpdated();
    }
}

void RendererDXR::render()
{
    if (!valid())
        return;

    if (m_mutex.try_lock()) {
        ++m_render_count;
        m_is_rendering = true;
        auto ctx = GfxContextDXR::getInstance();
        ctx->prepare(m_render_data);
        ctx->setSceneData(m_render_data, m_scene_data);
        ctx->setRenderTarget(m_render_data, m_render_target);
        ctx->setGeometries(m_render_data, m_geometries);
        ctx->flush(m_render_data);
        m_mutex.unlock();
    }
    else {
        ++m_skip_count;
    }
}

void RendererDXR::finish()
{
    if (!valid())
        return;

    auto ctx = GfxContextDXR::getInstance();
    if (!ctx->finish(m_render_data))
        m_render_data.clear();
    m_is_rendering = false;
}

void RendererDXR::frameEnd()
{
}

bool RendererDXR::readbackRenderTarget(void *dst)
{
    if (!valid())
        return false;

    auto ctx = GfxContextDXR::getInstance();
    return ctx->readbackRenderTarget(m_render_data, dst);
}

std::string RendererDXR::getTimestampLog()
{
    std::string ret;
#ifdef rthsEnableTimestamp
    if (m_render_data.timestamp) {
        if (m_mutex.try_lock()) {
            ret = m_render_data.timestamp->getLog();
            m_mutex.unlock();
        }
    }
#endif // rthsEnableTimestamp
    return ret;
}

void* RendererDXR::getRenderTexturePtr()
{
    if (m_render_data.render_target)
        return m_render_data.render_target->texture->resource.GetInterfacePtr();
    return nullptr;
}

IRenderer* CreateRendererDXR()
{
    auto ret = new RendererDXR();
    if (ret->initialized() && !ret->valid()) {
        ret->release();
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
