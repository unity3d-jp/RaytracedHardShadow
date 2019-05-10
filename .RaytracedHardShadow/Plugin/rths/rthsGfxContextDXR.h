#pragma once
#ifdef _WIN32
#include "rthsTypesDXR.h"

namespace rths {

class GfxContextDXR
{
public:
    static bool initializeInstance();
    static void finalizeInstance();
    static GfxContextDXR* getInstance();

    bool valid() const;
    ID3D12Device5* getDevice();

    TextureData translateTexture(void *ptr);
    void copyTexture(void *dst, ID3D12ResourcePtr src);
    BufferData translateVertexBuffer(void *ptr);
    BufferData translateIndexBuffer(void *ptr);
    BufferData allocateTransformBuffer(const float4x4& trans);

    void setRenderTarget(TextureData rt);
    void setMeshes(std::vector<MeshBuffers>& meshes);
    void flush();
    void finish();

private:
    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    D3D12_CPU_DESCRIPTOR_HANDLE createRTV(ID3D12ResourcePtr pResource, ID3D12DescriptorHeapPtr pHeap, uint32_t& usedHeapEntries, DXGI_FORMAT format);
    void addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    uint64_t submitCommandList();


private:
    GfxContextDXR();
    ~GfxContextDXR();

    ID3D12Device5Ptr m_device;
    ID3D12CommandAllocatorPtr m_cmd_allocator;
    ID3D12DescriptorHeapPtr m_desc_heap;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12CommandQueuePtr m_cmd_queue;
    ID3D12FencePtr m_fence;
    HANDLE m_fence_event;
    uint64_t m_fence_value = 0;

    ID3D12ResourcePtr m_toplevel_as;
    std::vector<ID3D12ResourcePtr> m_bottomlevel_as;
    std::vector<ID3D12ResourcePtr> m_temporary_buffers;

    TextureData m_render_target;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtv;

    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12RootSignaturePtr m_empty_rootsig;
    ID3D12ResourcePtr m_shader_table;
    uint32_t m_shader_table_entry_size = 0;

    ID3D12DescriptorHeapPtr m_srv_uav_heap;
    static const uint32_t kSrvUavHeapSize = 2;
};

const std::string& GetErrorLog();
void SetErrorLog(const char *format, ...);

} // namespace rths
#endif
