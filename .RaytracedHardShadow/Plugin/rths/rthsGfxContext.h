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
    BufferData allocateTransformBuffer(const float4x4& trans);

    void setRenderTarget(TextureData rt);
    void setMeshes(std::vector<MeshBuffers>& meshes);
    void flush();
    void finish();

private:
    struct AccelerationStructureBuffers
    {
        ID3D12ResourcePtr scratch;
        ID3D12ResourcePtr result;
        ID3D12ResourcePtr instance_desc; // used only for top-level AS
    };
    D3D12_CPU_DESCRIPTOR_HANDLE createRTV(ID3D12ResourcePtr pResource, ID3D12DescriptorHeapPtr pHeap, uint32_t& usedHeapEntries, DXGI_FORMAT format);
    void addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);


private:
    GfxContext();
    ~GfxContext();

    ID3D12Device5Ptr m_device;
    ID3D12CommandAllocatorPtr m_cmd_allocator;
    ID3D12DescriptorHeapPtr m_desc_heap;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12CommandQueuePtr m_cmd_queue;
    ID3D12FencePtr m_fence;
    HANDLE m_fence_event;
    uint64_t m_fence_value = 0;

    AccelerationStructureBuffers m_as_buffers;
    ID3D12ResourcePtr m_render_target;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtv;
};

const std::string& GetErrorLog();
void SetErrorLog(const char *format, ...);

} // namespace rths
