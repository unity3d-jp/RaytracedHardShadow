#pragma once

#include "rthsTypes.h"

namespace rths {

class GfxContext
{
public:
    static bool initializeInstance();
    static void finalizeInstance();
    static GfxContext* getInstance();

    bool valid() const;
    ID3D12Device5* getDevice();

    TextureData translateTexture(void *ptr);
    BufferData translateVertexBuffer(void *ptr);
    BufferData translateIndexBuffer(void *ptr);

private:
    struct AccelerationStructureBuffers
    {
        ID3D12ResourcePtr scratch;
        ID3D12ResourcePtr result;
        ID3D12ResourcePtr instance_desc; // used only for top-level AS
    };
    void updateAccelerationStructure(std::vector<MeshBuffers>& meshes);


private:
    GfxContext();
    ~GfxContext();

    ID3D12Device5Ptr m_device;
    ID3D12DescriptorHeapPtr m_desc_heap;
    ID3D12CommandQueuePtr m_cmd_queue;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12FencePtr m_fence;

    AccelerationStructureBuffers m_as_buffers;
};

const std::string& GetErrorLog();
void SetErrorLog(const char *format, ...);

} // namespace rths
