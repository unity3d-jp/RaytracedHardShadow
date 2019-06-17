#pragma once
#ifdef _WIN32
#include "rthsTypesDXR.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}

    virtual ID3D12FencePtr getFence(ID3D12Device *dxr_device) = 0;
    virtual void setFenceValue(uint64_t v) = 0;
    virtual uint64_t inclementFenceValue() = 0;

    virtual TextureDataDXRPtr createTemporaryTexture(void *ptr) = 0;
    virtual void applyTexture(TextureDataDXR& tex) = 0;
    virtual BufferDataDXRPtr translateBuffer(void *ptr) = 0;
};
using IResourceTranslatorPtr = std::shared_ptr<IResourceTranslator>;

IResourceTranslatorPtr CreateResourceTranslator();

} // namespace rths
#endif
