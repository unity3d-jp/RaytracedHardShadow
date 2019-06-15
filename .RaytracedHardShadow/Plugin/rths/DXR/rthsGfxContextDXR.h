#pragma once
#ifdef _WIN32
#include "rthsTypesDXR.h"
#include "rthsResourceTranslatorDXR.h"
#include "rthsDeformerDXR.h"

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
    ID3D12Device5Ptr getDevice();

    bool initializeDevice();
    void setSceneData(SceneData& data);
    void setRenderTarget(TextureData& rt);
    void setMeshes(std::vector<MeshInstanceData>& instances);
    void sync();
    void flush();
    void finish();
    void releaseUnusedResources();

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);

    void addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    uint64_t submitCommandList();
    bool readbackBuffer(void *dst, ID3D12Resource *src, size_t size);
    bool uploadBuffer(ID3D12Resource *dst, const void *src, size_t size);
    bool readbackTexture(void *dst, ID3D12Resource *src, size_t width, size_t height, size_t stride);
    bool uploadTexture(ID3D12Resource *dst, const void *src, size_t width, size_t height, size_t stride);
    void executeAndWaitCopy();

private:
    friend std::unique_ptr<GfxContextDXR> std::make_unique<GfxContextDXR>();
    friend struct std::default_delete<GfxContextDXR>;

    GfxContextDXR();
    ~GfxContextDXR();

    IResourceTranslatorPtr m_resource_translator;
    DeformerDXRPtr m_deformer;

    ID3D12Device5Ptr m_device;

    // command list for raytrace
    ID3D12CommandAllocatorPtr m_cmd_allocator;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12CommandQueuePtr m_cmd_queue;

    // command list for copy resources
    ID3D12CommandAllocatorPtr m_cmd_allocator_copy;
    ID3D12GraphicsCommandList4Ptr m_cmd_list_copy;
    ID3D12CommandQueuePtr m_cmd_queue_copy;

    ID3D12FencePtr m_fence;
    FenceEventDXR m_fence_event;

    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12RootSignaturePtr m_global_rootsig;
    ID3D12RootSignaturePtr m_local_rootsig;

    ID3D12ResourcePtr m_shader_table;
    int m_shader_table_entry_count = 0;
    int m_shader_table_entry_capacity = 0;
    int m_shader_record_size = 0;

    ID3D12DescriptorHeapPtr m_srvuav_heap;

    std::map<TextureData, TextureDataDXRPtr> m_texture_records;
    std::map<BufferData, BufferDataDXRPtr> m_buffer_records;
    std::map<MeshData, MeshDataDXRPtr> m_mesh_records;
    std::vector<ID3D12ResourcePtr> m_temporary_buffers;

    ID3D12ResourcePtr m_scene_buffer;
    DescriptorHandleDXR m_scene_buffer_handle;

    TextureDataDXRPtr m_render_target;
    DescriptorHandleDXR m_render_target_handle;

    std::vector<MeshInstanceDataDXR> m_mesh_instances;
    ID3D12ResourcePtr m_tlas;
    DescriptorHandleDXR m_tlas_handle;

    bool m_flushing = false;
};

} // namespace rths
#endif
