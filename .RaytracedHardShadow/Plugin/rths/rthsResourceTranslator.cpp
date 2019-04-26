#include "pch.h"
#include "rthsResourceTranslator.h"
#include "rthsGfxContext.h"

namespace rths {

class D3D11ResourceTranslator : public IResourceTranslator
{
public:
    D3D11ResourceTranslator(ID3D11Device *device);
    ~D3D11ResourceTranslator() override;
    ID3D12ResourcePtr translateTexture(void *ptr) override;
    ID3D12ResourcePtr translateVertexBuffer(void *ptr) override;
    ID3D12ResourcePtr translateIndexBuffer(void *ptr) override;

private:
    ID3D11Device *m_device;
};

class D3D12ResourceTranslator : public IResourceTranslator
{
public:
    D3D12ResourceTranslator(ID3D12Device *device);
    ~D3D12ResourceTranslator() override;
    ID3D12ResourcePtr translateTexture(void *ptr) override;
    ID3D12ResourcePtr translateVertexBuffer(void *ptr) override;
    ID3D12ResourcePtr translateIndexBuffer(void *ptr) override;

private:
    ID3D12Device *m_device;
};




D3D11ResourceTranslator::D3D11ResourceTranslator(ID3D11Device *device)
    : m_device(device)
{
}

D3D11ResourceTranslator::~D3D11ResourceTranslator()
{
}

ID3D12ResourcePtr D3D11ResourceTranslator::translateTexture(void *ptr)
{
    // todo
    return ID3D12ResourcePtr();
}

ID3D12ResourcePtr D3D11ResourceTranslator::translateVertexBuffer(void *ptr)
{
    HANDLE handle = nullptr;
    HRESULT hr = 0;

    auto d3d11_buf = (ID3D11Buffer*)ptr;
    IDXGIResource *ires = nullptr;
    hr = d3d11_buf->QueryInterface(__uuidof(IDXGIResource), (LPVOID*)&ires);
    hr = ires->GetSharedHandle(&handle);
    ires->Release();

    auto device = GfxContext::getInstance()->getDevice();
    ID3D12Resource *d3d12_res = nullptr;
    hr = device->OpenSharedHandle(handle, __uuidof(ID3D12Resource), (LPVOID*)&d3d12_res);

    return d3d12_res;
}

ID3D12ResourcePtr D3D11ResourceTranslator::translateIndexBuffer(void *ptr)
{
    // todo
    return ID3D12ResourcePtr();
}




D3D12ResourceTranslator::D3D12ResourceTranslator(ID3D12Device *device)
    : m_device(device)
{
}

D3D12ResourceTranslator::~D3D12ResourceTranslator()
{
}

ID3D12ResourcePtr D3D12ResourceTranslator::translateTexture(void * ptr)
{
    // todo
    return ID3D12ResourcePtr();
}

ID3D12ResourcePtr D3D12ResourceTranslator::translateVertexBuffer(void * ptr)
{
    // todo
    return ID3D12ResourcePtr();
}

ID3D12ResourcePtr D3D12ResourceTranslator::translateIndexBuffer(void * ptr)
{
    // todo
    return ID3D12ResourcePtr();
}


IResourceTranslator *g_resource_translator;

void InitializeResourceTranslator(ID3D11Device *d3d11)
{
}

void InitializeResourceTranslator(ID3D12Device *d3d12)
{

}

void FinalizeResourceTranslator()
{
    delete g_resource_translator;
    g_resource_translator = nullptr;
}

IResourceTranslator* GetResourceTranslator()
{
    return g_resource_translator;
}

} // namespace rths
