#pragma once
#ifdef _WIN32
#include "rthsTypesDXR.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}

    virtual ID3D12FencePtr getFence(ID3D12Device *dxr_device) = 0;
    virtual HANDLE getFenceEvent() = 0;
    virtual uint64_t inclementFenceValue() = 0;

    virtual TextureDataDXR createTemporaryTexture(void *ptr) = 0;
    virtual void applyTexture(TextureDataDXR& tex) = 0;
    virtual BufferDataDXR translateVertexBuffer(void *ptr) = 0;
    virtual BufferDataDXR translateIndexBuffer(void *ptr) = 0;
};

void InitializeResourceTranslator(ID3D11Device *unity_gfx_device);
void InitializeResourceTranslator(ID3D12Device *unity_gfx_device);
void FinalizeResourceTranslator();
IResourceTranslator* GetResourceTranslator();

} // namespace rths
#endif
