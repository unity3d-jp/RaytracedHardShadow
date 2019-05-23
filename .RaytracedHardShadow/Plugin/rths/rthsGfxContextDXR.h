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
    void setSceneData(SceneData& data);
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
    ID3D12ResourcePtr m_tlas;
    Descriptor m_tlas_handle;
    std::vector<ID3D12ResourcePtr> m_temporary_buffers;

    TextureDataDXR m_render_target;
    Descriptor m_render_target_handle;

    ID3D12ResourcePtr m_scene_buffer;
    Descriptor m_scene_buffer_handle;

    ID3D12ResourcePtr m_shader_table;
    int m_desc_heap_stride = 0;
    int m_shader_table_entry_count = 0;
    int m_shader_table_entry_capacity = 0;
    int m_shader_record_size = 0;

    ID3D12DescriptorHeapPtr m_srvuav_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvuav_cpu_handle_base;
    D3D12_GPU_DESCRIPTOR_HANDLE m_srvuav_gpu_handle_base;

    bool m_flushing = false;

#ifdef rthsDebug
    ID3D12ResourcePtr m_render_target_readback;
    ID3D12CommandAllocatorPtr m_cmd_allocator_copy;
    ID3D12GraphicsCommandList4Ptr m_cmd_list_copy;
    ID3D12CommandQueuePtr m_cmd_queue_copy;
#endif
};

} // namespace rths
#endif
