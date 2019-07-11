#include "pch.h"
#ifdef _WIN32
#include "rthsResourceTranslatorDXR.h"
#include "rthsGfxContextDXR.h"

namespace rths {

class ResourceTranslatorBase : public IResourceTranslator
{
protected:
    ID3D12ResourcePtr createTemporaryTextureImpl(int width, int height, DXGI_FORMAT format, bool shared);
};


class D3D11ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D11ResourceTranslator(ID3D11Device *device);
    ~D3D11ResourceTranslator() override;

    ID3D12FencePtr getFence(ID3D12Device *dxr_device) override;
    uint64_t insertSignal() override;

    TextureDataDXRPtr createTemporaryTexture(GPUResourcePtr ptr) override;
    uint64_t syncTexture(TextureDataDXR& tex, uint64_t fence_value) override;
    BufferDataDXRPtr translateBuffer(GPUResourcePtr ptr) override;

    bool isValidTexture(TextureDataDXR& data) override;
    bool isValidBuffer(BufferDataDXR& data) override;

    uint64_t copyResource(ID3D11Resource *dst, ID3D11Resource *src, bool immediate);

private:
    // note:
    // sharing resources from d3d12 to d3d11 require d3d11.1. also, fence is supported only d3d11.4 or newer.
    ID3D11Device5Ptr m_host_device;
    ID3D11DeviceContext4Ptr m_host_context;

    ID3D11FencePtr m_fence;
    FenceEventDXR m_fence_event;
};

class D3D12ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D12ResourceTranslator(ID3D12Device *device);
    ~D3D12ResourceTranslator() override;

    ID3D12FencePtr getFence(ID3D12Device *dxr_device) override;
    uint64_t insertSignal() override;

    TextureDataDXRPtr createTemporaryTexture(GPUResourcePtr ptr) override;
    uint64_t syncTexture(TextureDataDXR& tex, uint64_t fence_value) override;
    BufferDataDXRPtr translateBuffer(GPUResourcePtr ptr) override;

    bool isValidTexture(TextureDataDXR& data) override;
    bool isValidBuffer(BufferDataDXR& data) override;

private:
    ID3D12DevicePtr m_host_device;

    ID3D12FencePtr m_fence;
    FenceEventDXR m_fence_event;
};




ID3D12ResourcePtr ResourceTranslatorBase::createTemporaryTextureImpl(int width, int height, DXGI_FORMAT format, bool shared)
{
    // note: sharing textures with d3d11 requires some flags and restrictions:
    // - MipLevels must be 1
    // - D3D12_HEAP_FLAG_SHARED for heap flags
    // - D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET and D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS for resource flags

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;
    if (shared) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        flags |= D3D12_HEAP_FLAG_SHARED;
    }

    ID3D12ResourcePtr ret;
    auto hr = GfxContextDXR::getInstance()->getDevice()->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret));
    return ret;

}

D3D11ResourceTranslator::D3D11ResourceTranslator(ID3D11Device *device)
{
    device->QueryInterface(IID_PPV_ARGS(&m_host_device));

    ID3D11DeviceContextPtr device_context;
    m_host_device->GetImmediateContext(&device_context);
    device_context->QueryInterface(IID_PPV_ARGS(&m_host_context));

    m_host_device->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
}

D3D11ResourceTranslator::~D3D11ResourceTranslator()
{
}

ID3D12FencePtr D3D11ResourceTranslator::getFence(ID3D12Device *dxr_device)
{
    ID3D12FencePtr ret;
    HANDLE hfence;
    auto hr = m_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &hfence);
    if (SUCCEEDED(hr)) {
        hr = dxr_device->OpenSharedHandle(hfence, IID_PPV_ARGS(&ret));
    }
    return ret;
}

uint64_t D3D11ResourceTranslator::insertSignal()
{
    auto fv = GfxContextDXR::getInstance()->incrementFenceValue();
    m_host_context->Signal(m_fence, fv);
    return fv;
}


TextureDataDXRPtr D3D11ResourceTranslator::createTemporaryTexture(GPUResourcePtr ptr)
{
    auto ret = std::make_shared<TextureDataDXR>();

    auto tex_host = (ID3D11Texture2D*)ptr;
    ret->host_ptr = ptr;
    ret->host_d3d11 = tex_host;
    ret->initial_ref = GetRefCount(ret->host_d3d11);

    D3D11_TEXTURE2D_DESC src_desc{};
    tex_host->GetDesc(&src_desc);

    ret->width = src_desc.Width;
    ret->height = src_desc.Height;
    ret->format = src_desc.Format;
    ret->resource = createTemporaryTextureImpl(ret->width, ret->height, ret->format, true);

    auto hr = GfxContextDXR::getInstance()->getDevice()->CreateSharedHandle(ret->resource, nullptr, GENERIC_ALL, nullptr, &ret->handle);
    if (SUCCEEDED(hr)) {
        // note: ID3D11Device::OpenSharedHandle() doesn't accept handles created by d3d12. ID3D11Device1::OpenSharedHandle1() is needed.
        hr = m_host_device->OpenSharedResource1(ret->handle, IID_PPV_ARGS(&ret->temporary_d3d11));
        ret->is_nt_handle = true;
    }

#if 0
    {
        // create shareable texture from d3d11
        // (todo: creating resource and output result from DXR works but sharing content with d3d11 side seems don't work)
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = ret.width;
        desc.Height = ret.height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = ret.format;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        auto hr = m_host_device->CreateTexture2D(&desc, nullptr, &ret.temporary_d3d11);
        if (SUCCEEDED(hr)) {
            IDXGIResource1Ptr ires;
            hr = ret.temporary_d3d11->QueryInterface(IID_PPV_ARGS(&ires));
            if (SUCCEEDED(hr)) {
                hr = ires->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &ret.handle);
                if (SUCCEEDED(hr)) {
                    hr = GfxContextDXR::getInstance()->getDevice()->OpenSharedHandle(ret.handle, IID_PPV_ARGS(&ret.resource));
                }
            }
        }
    }
#endif
    return ret;
}

uint64_t D3D11ResourceTranslator::syncTexture(TextureDataDXR& src, uint64_t fence_value)
{
    if (src.temporary_d3d11) {
        m_host_context->Wait(m_fence, fence_value);
        fence_value = copyResource((ID3D11Texture2D*)src.host_ptr, src.temporary_d3d11, false);
        m_host_context->Signal(m_fence, fence_value);
        return fence_value;
    }
    return 0;
}

BufferDataDXRPtr D3D11ResourceTranslator::translateBuffer(GPUResourcePtr ptr)
{
    auto ret = std::make_shared<BufferDataDXR>();

    auto buf_host = (ID3D11Buffer*)ptr;
    ret->host_ptr = ptr;
    ret->host_d3d11 = buf_host;
    ret->initial_ref = GetRefCount(ret->host_d3d11);

    D3D11_BUFFER_DESC src_desc{};
    buf_host->GetDesc(&src_desc);

    // create temporary buffer that can be shared with DXR side
    D3D11_BUFFER_DESC tmp_desc = src_desc;
    tmp_desc.Usage = D3D11_USAGE_DEFAULT;
    tmp_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tmp_desc.CPUAccessFlags = 0;
    tmp_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    HRESULT hr = m_host_device->CreateBuffer(&tmp_desc, nullptr, &ret->temporary_d3d11);
    if (SUCCEEDED(hr)) {
        // copy contents to temporary
        m_host_context->CopyResource(ret->temporary_d3d11, buf_host);

        // translate temporary as d3d12 resource
        IDXGIResourcePtr ires;
        hr = ret->temporary_d3d11->QueryInterface(IID_PPV_ARGS(&ires));
        if (SUCCEEDED(hr)) {
            hr = ires->GetSharedHandle(&ret->handle); // note: this handle is *NOT* NT handle
            hr = GfxContextDXR::getInstance()->getDevice()->OpenSharedHandle(ret->handle, IID_PPV_ARGS(&ret->resource));
            ret->size = src_desc.ByteWidth;
        }
    }
    return ret;
}

bool D3D11ResourceTranslator::isValidTexture(TextureDataDXR& data)
{
    return GetRefCount(data.host_d3d11) >= data.initial_ref;
}

bool D3D11ResourceTranslator::isValidBuffer(BufferDataDXR& data)
{
    return GetRefCount(data.host_d3d11) >= data.initial_ref;
}

uint64_t D3D11ResourceTranslator::copyResource(ID3D11Resource *dst, ID3D11Resource *src, bool immediate)
{
    m_host_context->CopyResource(dst, src);

    auto fence_value = GfxContextDXR::getInstance()->incrementFenceValue();
    m_host_context->Signal(m_fence, fence_value);
    if (immediate) {
        // wait for completion of CopyResource()
        m_fence->SetEventOnCompletion(fence_value, m_fence_event);
        ::WaitForSingleObject(m_fence_event, kTimeoutMS);
    }
    return fence_value;
}




D3D12ResourceTranslator::D3D12ResourceTranslator(ID3D12Device *device)
    : m_host_device(device)
{
    m_host_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
}

D3D12ResourceTranslator::~D3D12ResourceTranslator()
{
}

ID3D12FencePtr D3D12ResourceTranslator::getFence(ID3D12Device *dxr_device)
{
    return m_fence;
}

uint64_t D3D12ResourceTranslator::insertSignal()
{
    // on d3d12 this can be skipped because no copy is needed for vertex buffers
    return 0;
}

TextureDataDXRPtr D3D12ResourceTranslator::createTemporaryTexture(GPUResourcePtr ptr)
{
    auto ret = std::make_shared<TextureDataDXR>();

    auto tex_host = (ID3D12Resource*)ptr;
    ret->host_ptr = ptr;
    ret->host_d3d12 = tex_host;
    ret->initial_ref = GetRefCount(ret->host_d3d12);

    D3D12_RESOURCE_DESC src_desc = tex_host->GetDesc();
    ret->width = (int)src_desc.Width;
    ret->height = (int)src_desc.Height;
    ret->format = src_desc.Format;

    // if unordered access is allowed, it can be directly used as DXR's result buffer. so temporary texture is not needed
    if ((src_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0) {
        ret->resource = tex_host;
    }
    else {
        ret->resource = createTemporaryTextureImpl(ret->width, ret->height, ret->format, false);
    }
    return ret;
}

uint64_t D3D12ResourceTranslator::syncTexture(TextureDataDXR& src, uint64_t fv)
{
    auto tex_host = (ID3D12Resource*)src.host_ptr;
    // copy is not needed if unordered access is allowed
    if (src.resource == tex_host)
        return 0;

    auto *inst = GfxContextDXR::getInstance();
    fv = inst->submitResourceBarrier(tex_host, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COMMON, fv);
    fv = inst->copyTexture(tex_host, src.resource, false, fv);
    fv = inst->submitResourceBarrier(tex_host, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ, fv);
    return fv;
}

BufferDataDXRPtr D3D12ResourceTranslator::translateBuffer(GPUResourcePtr ptr)
{
    auto ret = std::make_shared<BufferDataDXR>();

    auto buf_host = (ID3D12Resource*)ptr;
    ret->host_ptr = ptr;
    ret->host_d3d12 = buf_host;

    D3D12_RESOURCE_DESC src_desc = buf_host->GetDesc();
    // on d3d12, buffer can be directly shared with DXR side
    ret->resource = buf_host;
    ret->size = (int)src_desc.Width;
    ret->initial_ref = GetRefCount(ret->host_d3d12);
    return ret;
}

bool D3D12ResourceTranslator::isValidTexture(TextureDataDXR& data)
{
    return GetRefCount(data.host_d3d12) >= data.initial_ref;
}

bool D3D12ResourceTranslator::isValidBuffer(BufferDataDXR& data)
{
    return GetRefCount(data.host_d3d12) >= data.initial_ref;
}


ID3D11Device *g_host_d3d11_device;
ID3D12Device *g_host_d3d12_device;

IResourceTranslatorPtr CreateResourceTranslator()
{
    if (g_host_d3d12_device)
        return std::make_shared<D3D12ResourceTranslator>(g_host_d3d12_device);
    if (g_host_d3d11_device)
        return std::make_shared<D3D11ResourceTranslator>(g_host_d3d11_device);
    return nullptr;
}

} // namespace rths
#endif // _WIN32
