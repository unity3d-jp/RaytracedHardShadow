#pragma once
#include "rthsTypes.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}
    virtual ID3D12ResourcePtr createTemporaryRenderTarget(void *ptr) = 0;
    virtual void copyRenderTarget(void *dst, ID3D12ResourcePtr src) = 0;
    virtual ID3D12ResourcePtr translateVertexBuffer(void *ptr) = 0;
    virtual ID3D12ResourcePtr translateIndexBuffer(void *ptr) = 0;
};

void InitializeResourceTranslator(ID3D11Device *unity_gfx_device);
void InitializeResourceTranslator(ID3D12Device *unity_gfx_device);
void FinalizeResourceTranslator();
IResourceTranslator* GetResourceTranslator(ID3D12Device *my_gfx_device);

} // namespace rths
