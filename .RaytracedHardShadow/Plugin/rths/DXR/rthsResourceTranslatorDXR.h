#pragma once
#ifdef _WIN32
#include "rthsTypesDXR.h"

namespace rths {

class IResourceTranslator
{
public:
    virtual ~IResourceTranslator() {}

    virtual ID3D12FencePtr getFence(ID3D12Device *dxr_device) = 0;
    virtual uint64_t insertSignal() = 0;

    virtual TextureDataDXRPtr createTemporaryTexture(GPUResourcePtr ptr) = 0;
    virtual uint64_t syncTexture(TextureDataDXR& tex, uint64_t fence_value_to_wait) = 0;
    virtual BufferDataDXRPtr translateBuffer(GPUResourcePtr ptr) = 0;

    virtual bool isValidTexture(TextureDataDXR& data) = 0;
    virtual bool isValidBuffer(BufferDataDXR& data) = 0;
};
using IResourceTranslatorPtr = std::shared_ptr<IResourceTranslator>;

IResourceTranslatorPtr CreateResourceTranslator();

} // namespace rths
#endif
