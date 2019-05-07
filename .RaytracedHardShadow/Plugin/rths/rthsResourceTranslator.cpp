#include "pch.h"
#include "rthsResourceTranslator.h"
#include "rthsGfxContext.h"

namespace rths {

class ResourceTranslatorBase : public IResourceTranslator
{
public:

protected:
    std::map<void*, ID3D12ResourcePtr> m_render_target_table;
    std::map<void*, ID3D12ResourcePtr> m_buffer_table;
};


class D3D11ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D11ResourceTranslator(ID3D11Device *device);
    ~D3D11ResourceTranslator() override;
    ID3D12ResourcePtr createTemporaryRenderTarget(void *ptr) override;
    void copyRenderTarget(void *dst, ID3D12ResourcePtr src) override;
    ID3D12ResourcePtr translateVertexBuffer(void *ptr) override;
    ID3D12ResourcePtr translateIndexBuffer(void *ptr) override;

private:
    ID3D11Device *m_unity_device = nullptr;
};

class D3D12ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D12ResourceTranslator(ID3D12Device *device);
    ~D3D12ResourceTranslator() override;
    ID3D12ResourcePtr createTemporaryRenderTarget(void *ptr) override;
    void copyRenderTarget(void *dst, ID3D12ResourcePtr src) override;
    ID3D12ResourcePtr translateVertexBuffer(void *ptr) override;
    ID3D12ResourcePtr translateIndexBuffer(void *ptr) override;

private:
    ID3D12Device *m_device = nullptr;
    ID3D12Device *m_unity_device = nullptr;
};




D3D11ResourceTranslator::D3D11ResourceTranslator(ID3D11Device *device)
    : m_unity_device(device)
{
}

D3D11ResourceTranslator::~D3D11ResourceTranslator()
{
}

ID3D12ResourcePtr D3D11ResourceTranslator::createTemporaryRenderTarget(void *ptr)
{
    auto& ret = m_render_target_table[ptr];
    if (ret)
        return ret;

    auto tex = (ID3D11Texture2D*)ptr;
    D3D11_TEXTURE2D_DESC src_desc{};
    tex->GetDesc(&src_desc);

    D3D12_HEAP_PROPERTIES prop{};
    prop.Type = D3D12_HEAP_TYPE_DEFAULT;
    prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    prop.CreationNodeMask = 1;
    prop.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = src_desc.Width;
    desc.Height = src_desc.Height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES | D3D12_HEAP_FLAG_SHARED;

    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    D3D12_CLEAR_VALUE clear_value{};
    clear_value.Format = desc.Format;

    auto device = GfxContext::getInstance()->getDevice();
    auto hr = device->CreateCommittedResource(&prop, flags, &desc, initial_state, &clear_value, IID_PPV_ARGS(&ret));

    return ret;
}

void D3D11ResourceTranslator::copyRenderTarget(void *dst, ID3D12ResourcePtr src)
{
}

ID3D12ResourcePtr D3D11ResourceTranslator::translateVertexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret)
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
    ID3D11DeviceContext *ctx = nullptr;
    m_unity_device->GetImmediateContext(&ctx);
    ctx->CopyResource(tmp_buf, d3d11_buf);

    // translate temporary as d3d12 resource
    IDXGIResource *ires = nullptr;
    hr = tmp_buf->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&ires);
    hr = ires->GetSharedHandle(&handle);
    ires->Release();

    auto device = GfxContext::getInstance()->getDevice();
    hr = device->OpenSharedHandle(handle, __uuidof(ID3D12Resource), (LPVOID*)&ret);

    return ret;
}

ID3D12ResourcePtr D3D11ResourceTranslator::translateIndexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret)
        return ret;

    // todo
    return ret;
}




D3D12ResourceTranslator::D3D12ResourceTranslator(ID3D12Device *device)
    : m_unity_device(device)
{
}

D3D12ResourceTranslator::~D3D12ResourceTranslator()
{
}

ID3D12ResourcePtr D3D12ResourceTranslator::createTemporaryRenderTarget(void *ptr)
{
    auto& ret = m_render_target_table[ptr];
    if (ret)
        return ret;

    // todo
    return ret;
}

void D3D12ResourceTranslator::copyRenderTarget(void * dst, ID3D12ResourcePtr src)
{
}

ID3D12ResourcePtr D3D12ResourceTranslator::translateVertexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret)
        return ret;

    // todo
    return ret;
}

ID3D12ResourcePtr D3D12ResourceTranslator::translateIndexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret)
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
