#include "pch.h"
#ifdef _WIN32
#include "rthsResourceTranslatorDXR.h"
#include "rthsGfxContextDXR.h"

namespace rths {

class ResourceTranslatorBase : public IResourceTranslator
{
protected:
    ID3D12ResourcePtr createTemporaryRenderTargetImpl(int width, int height);

protected:
    struct BufferHolder
    {
        ID3D12ResourcePtr dxr_buf;

    };

    std::map<void*, TextureData> m_render_target_table;
    std::map<void*, BufferData> m_buffer_table;
};


class D3D11ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D11ResourceTranslator(ID3D11Device *device);
    ~D3D11ResourceTranslator() override;
    TextureData createTemporaryRenderTarget(void *ptr) override;
    void copyTexture(void *dst, ID3D12ResourcePtr src) override;
    BufferData translateVertexBuffer(void *ptr) override;
    BufferData translateIndexBuffer(void *ptr) override;

private:
    // note: sharing resources from d3d12 to d3d11 require d3d11.1.
    ID3D11Device1Ptr m_unity_device = nullptr;
    ID3D11DeviceContextPtr m_unity_dev_context = nullptr;
};

class D3D12ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D12ResourceTranslator(ID3D12Device *device);
    ~D3D12ResourceTranslator() override;
    TextureData createTemporaryRenderTarget(void *ptr) override;
    void copyTexture(void *dst, ID3D12ResourcePtr src) override;
    BufferData translateVertexBuffer(void *ptr) override;
    BufferData translateIndexBuffer(void *ptr) override;

private:
    ID3D12Device *m_device = nullptr;
    ID3D12Device *m_unity_device = nullptr;
};




ID3D12ResourcePtr ResourceTranslatorBase::createTemporaryRenderTargetImpl(int width, int height)
{
    // note: sharing the resource with d3d11 requires some flags and restrictions:
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
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_SHARED;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COPY_SOURCE;

    auto device = GfxContextDXR::getInstance()->getDevice();
    ID3D12ResourcePtr ret;
    auto hr = device->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret));
    return ret;

}

D3D11ResourceTranslator::D3D11ResourceTranslator(ID3D11Device *device)
{
    device->QueryInterface(IID_PPV_ARGS(&m_unity_device));
    m_unity_device->GetImmediateContext(&m_unity_dev_context);
}

D3D11ResourceTranslator::~D3D11ResourceTranslator()
{
}

TextureData D3D11ResourceTranslator::createTemporaryRenderTarget(void *ptr)
{
    auto& ret = m_render_target_table[ptr];
    if (ret.resource)
        return ret;

    auto tex = (ID3D11Texture2D*)ptr;
    D3D11_TEXTURE2D_DESC src_desc{};
    tex->GetDesc(&src_desc);

    ret.width = src_desc.Width;
    ret.height = src_desc.Height;
    ret.resource = createTemporaryRenderTargetImpl(src_desc.Width, src_desc.Height);
    return ret;
}

void D3D11ResourceTranslator::copyTexture(void *dst, ID3D12ResourcePtr src)
{
    auto device = GfxContextDXR::getInstance()->getDevice();

    HANDLE handle = nullptr;
    HRESULT hr = device->CreateSharedHandle(src, nullptr, GENERIC_ALL, nullptr, &handle);

    // note: ID3D11Device::OpenSharedHandle() doesn't accept handles created by d3d12. ID3D11Device1::OpenSharedHandle1() is needed.
    ID3D11Texture2DPtr tex;
    hr = m_unity_device->OpenSharedResource1(handle, IID_PPV_ARGS(&tex));

    auto dst_d3d11 = (ID3D11Texture2D*)dst;
    m_unity_dev_context->CopyResource(dst_d3d11, tex);
}

BufferData D3D11ResourceTranslator::translateVertexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;

    HANDLE handle = nullptr;
    HRESULT hr = 0;

    auto d3d11_buf = (ID3D11Buffer*)ptr;
    D3D11_BUFFER_DESC src_desc{};
    d3d11_buf->GetDesc(&src_desc);

    // create temporary buffer that can be shared with d3d12
    D3D11_BUFFER_DESC tmp_desc = src_desc;
    tmp_desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
    ID3D11BufferPtr tmp_buf;
    hr = m_unity_device->CreateBuffer(&tmp_desc, nullptr, &tmp_buf);

    // copy contents of d3d11_buf to tmp_buf
    m_unity_dev_context->CopyResource(tmp_buf, d3d11_buf);

    // translate temporary as d3d12 resource
    IDXGIResourcePtr ires;
    hr = tmp_buf->QueryInterface(IID_PPV_ARGS(&ires));
    hr = ires->GetSharedHandle(&handle);

    auto device = GfxContextDXR::getInstance()->getDevice();
    hr = device->OpenSharedHandle(handle, IID_PPV_ARGS(&ret.resource));
    ret.size = src_desc.ByteWidth;

    return ret;
}

BufferData D3D11ResourceTranslator::translateIndexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;

    HANDLE handle = nullptr;
    HRESULT hr = 0;

    auto d3d11_buf = (ID3D11Buffer*)ptr;
    D3D11_BUFFER_DESC src_desc{};
    d3d11_buf->GetDesc(&src_desc);

    // create temporary buffer that can be shared with d3d12
    D3D11_BUFFER_DESC tmp_desc = src_desc;
    tmp_desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
    ID3D11BufferPtr tmp_buf;
    hr = m_unity_device->CreateBuffer(&tmp_desc, nullptr, &tmp_buf);

    // copy contents of d3d11_buf to tmp_buf
    m_unity_dev_context->CopyResource(tmp_buf, d3d11_buf);

    // translate temporary as d3d12 resource
    IDXGIResourcePtr ires;
    hr = tmp_buf->QueryInterface(IID_PPV_ARGS(&ires));
    hr = ires->GetSharedHandle(&handle);

    auto device = GfxContextDXR::getInstance()->getDevice();
    hr = device->OpenSharedHandle(handle, IID_PPV_ARGS(&ret.resource));
    ret.size = src_desc.ByteWidth;

    return ret;
}




D3D12ResourceTranslator::D3D12ResourceTranslator(ID3D12Device *device)
    : m_unity_device(device)
{
}

D3D12ResourceTranslator::~D3D12ResourceTranslator()
{
}

TextureData D3D12ResourceTranslator::createTemporaryRenderTarget(void *ptr)
{
    auto& ret = m_render_target_table[ptr];
    if (ret.resource)
        return ret;

    // todo
    return ret;
}

void D3D12ResourceTranslator::copyTexture(void * dst, ID3D12ResourcePtr src)
{
}

BufferData D3D12ResourceTranslator::translateVertexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;

    // todo
    return ret;
}

BufferData D3D12ResourceTranslator::translateIndexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;

    // todo
    return ret;
}


IResourceTranslator *g_resource_translator;

void InitializeResourceTranslator(ID3D11Device *d3d11)
{
    g_resource_translator = new D3D11ResourceTranslator(d3d11);
}

void InitializeResourceTranslator(ID3D12Device *d3d12)
{
    g_resource_translator = new D3D12ResourceTranslator(d3d12);
}

void FinalizeResourceTranslator()
{
    delete g_resource_translator;
    g_resource_translator = nullptr;
}

IResourceTranslator* GetResourceTranslator(ID3D12Device *my)
{
    return g_resource_translator;
}

} // namespace rths
#endif
