#include "pch.h"
#ifdef _WIN32
#include "rthsResourceTranslatorDXR.h"
#include "rthsGfxContextDXR.h"

namespace rths {

class ResourceTranslatorBase : public IResourceTranslator
{
protected:
    void clearCache() override;
    ID3D12ResourcePtr createTemporaryTextureImpl(int width, int height);

protected:
    std::map<void*, TextureDataDXR> m_render_target_table;
    std::map<void*, BufferDataDXR> m_buffer_table;
};


class D3D11ResourceTranslator : public ResourceTranslatorBase
{
public:
    D3D11ResourceTranslator(ID3D11Device *device);
    ~D3D11ResourceTranslator() override;
    TextureDataDXR& createTemporaryTexture(void *ptr) override;
    void applyTexture(TextureDataDXR& tex) override;
    BufferDataDXR& translateVertexBuffer(void *ptr) override;
    BufferDataDXR& translateIndexBuffer(void *ptr) override;

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
    TextureDataDXR& createTemporaryTexture(void *ptr) override;
    void applyTexture(TextureDataDXR& tex) override;
    BufferDataDXR& translateVertexBuffer(void *ptr) override;
    BufferDataDXR& translateIndexBuffer(void *ptr) override;

private:
    ID3D12Device *m_device = nullptr;
    ID3D12Device *m_unity_device = nullptr;
};




void ResourceTranslatorBase::clearCache()
{
    m_render_target_table.clear();
    m_buffer_table.clear();
}

ID3D12ResourcePtr ResourceTranslatorBase::createTemporaryTextureImpl(int width, int height)
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
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_SHARED;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    ID3D12ResourcePtr ret;
    auto hr = GfxContextDXR::getInstance()->getDevice()->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret));
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

TextureDataDXR& D3D11ResourceTranslator::createTemporaryTexture(void *ptr)
{
    auto& ret = m_render_target_table[ptr];
    if (ret.resource)
        return ret;

    auto tex_unity = (ID3D11Texture2D*)ptr;
    D3D11_TEXTURE2D_DESC src_desc{};
    tex_unity->GetDesc(&src_desc);

    ret.texture = ptr;
    ret.width = src_desc.Width;
    ret.height = src_desc.Height;
    ret.resource = createTemporaryTextureImpl(src_desc.Width, src_desc.Height);

    auto hr = GfxContextDXR::getInstance()->getDevice()->CreateSharedHandle(ret.resource, nullptr, GENERIC_ALL, nullptr, &ret.handle);
    if (SUCCEEDED(hr)) {
        // note: ID3D11Device::OpenSharedHandle() doesn't accept handles created by d3d12. ID3D11Device1::OpenSharedHandle1() is needed.
        hr = m_unity_device->OpenSharedResource1(ret.handle, IID_PPV_ARGS(&ret.temporary_d3d11));
    }
    return ret;
}

void D3D11ResourceTranslator::applyTexture(TextureDataDXR& src)
{
    if (src.temporary_d3d11) {
        m_unity_dev_context->CopyResource((ID3D11Texture2D*)src.texture, src.temporary_d3d11);
    }
}

BufferDataDXR& D3D11ResourceTranslator::translateVertexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;
    ret.buffer = ptr;

    auto buf_unity = (ID3D11Buffer*)ptr;
    D3D11_BUFFER_DESC src_desc{};
    buf_unity->GetDesc(&src_desc);

    // d3d12 shared buffer (seems don't work...)
#if 0
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = src_desc.ByteWidth;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_SHARED;
        D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COPY_DEST;

        auto hr = GfxContextDXR::getInstance()->getDevice()->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret.resource));
        if (SUCCEEDED(hr)) {
            hr = GfxContextDXR::getInstance()->getDevice()->CreateSharedHandle(ret.resource, nullptr, GENERIC_ALL, nullptr, &ret.handle);
            if (SUCCEEDED(hr)) {
                hr = m_unity_device->OpenSharedResource1(ret.handle, IID_PPV_ARGS(&ret.temporary_d3d11));
                if (SUCCEEDED(hr)) {
                    m_unity_dev_context->CopyResource((ID3D11Buffer*)ret.buffer, ret.temporary_d3d11);
                }
            }
        }
        return ret;
    }
#endif

    // d3d11 shared buffer (seems don't work...)
#if 0
    {
        // create temporary buffer that can be shared with d3d12
        D3D11_BUFFER_DESC tmp_desc = src_desc;
        tmp_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
        HRESULT hr = m_unity_device->CreateBuffer(&tmp_desc, nullptr, &ret.temporary_d3d11);
        if (SUCCEEDED(hr)) {
            // copy contents of VB to temporary
            m_unity_dev_context->CopyResource(ret.temporary_d3d11, buf_unity);

            D3D11_MAPPED_SUBRESOURCE mapped;
            m_unity_dev_context->Map(ret.temporary_d3d11, 0, D3D11_MAP_READ, 0, &mapped);
            m_unity_dev_context->Unmap(ret.temporary_d3d11, 0);

            // translate temporary as d3d12 resource
            IDXGIResourcePtr ires;
            hr = ret.temporary_d3d11->QueryInterface(IID_PPV_ARGS(&ires));
            hr = ires->GetSharedHandle(&ret.handle);
            hr = GfxContextDXR::getInstance()->getDevice()->OpenSharedHandle(ret.handle, IID_PPV_ARGS(&ret.resource));

            ret.size = src_desc.ByteWidth;
        }
        return ret;
    }
#endif

    // copy data via CPU (very slow but works)
#if 1
    {
        D3D11_BUFFER_DESC tmp_desc = src_desc;
        tmp_desc.Usage = D3D11_USAGE_STAGING;
        tmp_desc.BindFlags = 0;
        tmp_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        ID3D11BufferPtr tmp_buf;
        HRESULT hr = m_unity_device->CreateBuffer(&tmp_desc, nullptr, &tmp_buf);
        if (SUCCEEDED(hr)) {
            m_unity_dev_context->CopyResource(tmp_buf, buf_unity);

            D3D11_MAPPED_SUBRESOURCE mapped;
            hr = m_unity_dev_context->Map(tmp_buf, 0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(hr)) {
                auto ctx = GfxContextDXR::getInstance();
                ret.resource = ctx->createBuffer(src_desc.ByteWidth, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
                ctx->uploadBuffer(ret.resource, mapped.pData, src_desc.ByteWidth);
                // state must be D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                ctx->addResourceBarrier(ret.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                ret.size = src_desc.ByteWidth;

                m_unity_dev_context->Unmap(tmp_buf, 0);
            }
        }
        return ret;
    }
#endif
}

BufferDataDXR& D3D11ResourceTranslator::translateIndexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;
    ret.buffer = ptr;

    HRESULT hr = 0;

    auto buf_unity = (ID3D11Buffer*)ptr;
    D3D11_BUFFER_DESC src_desc{};
    buf_unity->GetDesc(&src_desc);

    // d3d11 shared buffer (seems don't work...)
#if 0
    {
        D3D11_BUFFER_DESC tmp_desc = src_desc;
        tmp_desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
        hr = m_unity_device->CreateBuffer(&tmp_desc, nullptr, &ret.temporary_d3d11);
        if (SUCCEEDED(hr)) {
            // copy contents of IB to temporary
            m_unity_dev_context->CopyResource(ret.temporary_d3d11, buf_unity);

            // translate temporary as d3d12 resource
            IDXGIResourcePtr ires;
            hr = ret.temporary_d3d11->QueryInterface(IID_PPV_ARGS(&ires));
            hr = ires->GetSharedHandle(&ret.handle);

            auto device = GfxContextDXR::getInstance()->getDevice();
            hr = device->OpenSharedHandle(ret.handle, IID_PPV_ARGS(&ret.resource));
            ret.size = src_desc.ByteWidth;
        }
        return ret;
    }
#endif

    // copy data via CPU (very slow but works)
#if 1
    {
        D3D11_BUFFER_DESC tmp_desc = src_desc;
        tmp_desc.Usage = D3D11_USAGE_STAGING;
        tmp_desc.BindFlags = 0;
        tmp_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        ID3D11BufferPtr tmp_buf;
        HRESULT hr = m_unity_device->CreateBuffer(&tmp_desc, nullptr, &tmp_buf);
        if (SUCCEEDED(hr)) {
            m_unity_dev_context->CopyResource(tmp_buf, buf_unity);

            D3D11_MAPPED_SUBRESOURCE mapped;
            hr = m_unity_dev_context->Map(tmp_buf, 0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(hr)) {
                auto ctx = GfxContextDXR::getInstance();
                ret.resource = ctx->createBuffer(src_desc.ByteWidth, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON, kDefaultHeapProps);
                ctx->uploadBuffer(ret.resource, mapped.pData, src_desc.ByteWidth);
                // state must be D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                ctx->addResourceBarrier(ret.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                ret.size = src_desc.ByteWidth;

                m_unity_dev_context->Unmap(tmp_buf, 0);
            }
        }
        return ret;
    }
#endif
}




D3D12ResourceTranslator::D3D12ResourceTranslator(ID3D12Device *device)
    : m_unity_device(device)
{
}

D3D12ResourceTranslator::~D3D12ResourceTranslator()
{
}

TextureDataDXR& D3D12ResourceTranslator::createTemporaryTexture(void *ptr)
{
    auto& ret = m_render_target_table[ptr];
    if (ret.resource)
        return ret;

    // todo
    return ret;
}

void D3D12ResourceTranslator::applyTexture(TextureDataDXR& src)
{
}

BufferDataDXR& D3D12ResourceTranslator::translateVertexBuffer(void *ptr)
{
    auto& ret = m_buffer_table[ptr];
    if (ret.resource)
        return ret;

    // todo
    return ret;
}

BufferDataDXR& D3D12ResourceTranslator::translateIndexBuffer(void *ptr)
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
