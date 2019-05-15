#pragma once
#ifdef _WIN32
#include "rthsTypesDXR.h"

#define rthsMaxBounce 2

namespace rths {

class GfxContextDXR
{
public:
    static bool initializeInstance();
    static void finalizeInstance();
    static GfxContextDXR* getInstance();

    bool valid() const;
    bool validateDevice();
    ID3D12Device5* getDevice();

    bool initializeDevice();
    void setRenderTarget(TextureDataDXR rt);
    void setMeshes(std::vector<MeshBuffersDXR>& meshes);
    void sync();
    void flush();
    void finish();

private:
    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    void addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    uint64_t submitCommandList();


private:
    GfxContextDXR();
    ~GfxContextDXR();

    ID3D12Device5Ptr m_device;
    ID3D12CommandAllocatorPtr m_cmd_allocator;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12CommandQueuePtr m_cmd_queue;
    ID3D12FencePtr m_fence;
    HANDLE m_fence_event;
    uint64_t m_fence_value = 0;

    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12RootSignaturePtr m_global_rootsig;
    ID3D12RootSignaturePtr m_local_rootsig;

    std::vector<MeshBuffersDXR> m_meshes;
    ID3D12ResourcePtr m_toplevel_as;
    std::vector<ID3D12ResourcePtr> m_temporary_buffers;

    TextureDataDXR m_render_target;
    ID3D12ResourcePtr m_scene_data;

    ID3D12ResourcePtr m_shader_table;
    int m_desc_heap_stride = 0;
    int m_shader_table_entry_size = 0;
    int m_shader_table_entry_count = 0;
    int m_shader_table_entry_capacity = 0;

    ID3D12DescriptorHeapPtr m_srv_uav_heap;
    static const int kSrvUavHeapSize = 2;

    bool m_flushing = false;
};

} // namespace rths
#endif
