#include "pch.h"
#include "rthsGfxContext.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}
    virtual ID3D12ResourcePtr translate(void *ptr) = 0;
};

class D3D11ResourceTranslator : public IResourceTranslator
{
public:
    ~D3D11ResourceTranslator() override;
    ID3D12ResourcePtr translate(void *ptr) override;
};

class D3D12ResourceTranslator : public IResourceTranslator
{
public:
    ~D3D12ResourceTranslator() override;
    ID3D12ResourcePtr translate(void *ptr) override;
};


static std::string g_gfx_error_log;
static std::once_flag g_gfx_once;
static GfxContext *g_gfx_context;
static IResourceTranslator *g_translator;

const std::string& GetErrorLog()
{
    return g_gfx_error_log;
}
void SetErrorLog(const char *format, ...)
{
    const int MaxBuf = 2048;
    char buf[MaxBuf];

    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    g_gfx_error_log = buf;
    va_end(args);
}


D3D12ResourceTranslator::~D3D12ResourceTranslator()
{
}

ID3D12ResourcePtr D3D12ResourceTranslator::translate(void * ptr)
{
    // todo
    return ID3D12ResourcePtr();
}

D3D11ResourceTranslator::~D3D11ResourceTranslator()
{
}

ID3D12ResourcePtr D3D11ResourceTranslator::translate(void * ptr)
{
    // todo
    return ID3D12ResourcePtr();
}


bool GfxContext::initializeInstance()
{
    std::call_once(g_gfx_once, []() {
        if (!g_translator)
            return;

        g_gfx_context = new GfxContext();
        if (!g_gfx_context->valid()) {
            delete g_gfx_context;
            g_gfx_context = nullptr;
        }
    });
    return g_gfx_context != nullptr;
}

void GfxContext::finalizeInstance()
{
    delete g_gfx_context;
    g_gfx_context = nullptr;
}


GfxContext* GfxContext::getInstance()
{
    return g_gfx_context;
}

GfxContext::GfxContext()
{
    IDXGIFactory4Ptr dxgi_factory;
    ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));

    // Find the HW adapter
    IDXGIAdapter1Ptr adapter;
    for (uint32_t i = 0; dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip SW adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Create the device
        ::D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
        HRESULT hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
        if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
            SetErrorLog("DXR is not supported on this device");
        }
    }
}

GfxContext::~GfxContext()
{
}

bool GfxContext::valid() const
{
    return m_device != nullptr;
}

ID3D12ResourcePtr GfxContext::translateResource(void * ptr)
{
    if (g_translator)
        return g_translator->translate(ptr);
    return nullptr;
}

} // namespace rths


// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    using namespace rths;

    auto* graphics = unityInterfaces->Get<IUnityGraphics>();
    switch (graphics->GetRenderer()) {
    case kUnityGfxRendererD3D11:
        g_translator = new D3D11ResourceTranslator();
        break;
    case kUnityGfxRendererD3D12:
        g_translator = new D3D12ResourceTranslator();
        break;
    default:
        // graphics API not supported
        SetErrorLog("Graphics API must be D3D11 or D3D12");
        break;
    }
}