#pragma once
#include "rthsTypes.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}
    virtual ID3D12ResourcePtr translateTexture(void *ptr) = 0;
    virtual ID3D12ResourcePtr translateVertexBuffer(void *ptr) = 0;
    virtual ID3D12ResourcePtr translateIndexBuffer(void *ptr) = 0;
};

void InitializeResourceTranslator(ID3D11Device *d3d11);
void InitializeResourceTranslator(ID3D12Device *d3d12);
void FinalizeResourceTranslator();
IResourceTranslator* GetResourceTranslator();

} // namespace rths
