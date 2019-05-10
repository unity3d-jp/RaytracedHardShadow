#pragma once
#include "rthsTypesDXR.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}
    virtual TextureData createTemporaryRenderTarget(void *ptr) = 0;
    virtual void copyTexture(void *dst, ID3D12ResourcePtr src) = 0;
    virtual BufferData translateVertexBuffer(void *ptr) = 0;
    virtual BufferData translateIndexBuffer(void *ptr) = 0;
};

void InitializeResourceTranslator(ID3D11Device *unity_gfx_device);
void InitializeResourceTranslator(ID3D12Device *unity_gfx_device);
void FinalizeResourceTranslator();
IResourceTranslator* GetResourceTranslator(ID3D12Device *my_gfx_device);

} // namespace rths
