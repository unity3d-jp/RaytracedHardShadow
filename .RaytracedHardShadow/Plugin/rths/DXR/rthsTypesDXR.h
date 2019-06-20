#pragma once

#include "rthsTypes.h"

#ifdef _WIN32
#ifdef rthsDebug
    // debug layer
    #define rthsEnableD3D12DebugLayer

    // GPU based validation
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/using-d3d12-debug-layer-gpu-based-validation
    #define rthsEnableD3D12GBV

    // DREAD (this requires Windows SDK 10.0.18362.0 or newer)
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/use-dred
    #define rthsEnableD3D12DREAD

    #define rthsEnableTimestamp

    #define rthsEnableResourceName

    //// VB & IB validation
    //#define rthsEnableBufferValidation

    //// render target validation
    //#define rthsEnableRenderTargetValidation
#endif


namespace rths {

#define DefPtr(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
DefPtr(IDXGISwapChain3);
DefPtr(IDXGIFactory4);
DefPtr(IDXGIAdapter1);
DefPtr(IDXGIResource);
DefPtr(IDXGIResource1);
DefPtr(IDxcBlobEncoding);
DefPtr(ID3D11Device);
DefPtr(ID3D11Device5);
DefPtr(ID3D11DeviceContext);
DefPtr(ID3D11DeviceContext4);
DefPtr(ID3D11Buffer);
DefPtr(ID3D11Texture2D);
DefPtr(ID3D11Query);
DefPtr(ID3D11Fence);
DefPtr(ID3D12Device);
DefPtr(ID3D12Device5);
DefPtr(ID3D12GraphicsCommandList4);
DefPtr(ID3D12CommandQueue);
DefPtr(ID3D12Fence);
DefPtr(ID3D12CommandAllocator);
DefPtr(ID3D12Resource);
DefPtr(ID3D12DescriptorHeap);
DefPtr(ID3D12StateObject);
DefPtr(ID3D12PipelineState);
DefPtr(ID3D12RootSignature);
DefPtr(ID3D12StateObjectProperties);
DefPtr(ID3D12QueryHeap);
DefPtr(ID3D12Debug);
#ifdef rthsEnableD3D12GBV
    DefPtr(ID3D12Debug1);
#endif
#ifdef rthsEnableD3D12DREAD
    DefPtr(ID3D12DeviceRemovedExtendedDataSettings);
    DefPtr(ID3D12DeviceRemovedExtendedData);
#endif
DefPtr(ID3DBlob);
DefPtr(IDxcCompiler);
DefPtr(IDxcLibrary);
DefPtr(IDxcBlobEncoding);
DefPtr(IDxcOperationResult);
DefPtr(IDxcBlob);
#undef DefPtr

struct DescriptorHandleDXR
{
    D3D12_CPU_DESCRIPTOR_HANDLE hcpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE hgpu{};

    operator bool() const;
};

class DescriptorHeapAllocatorDXR
{
public:
    DescriptorHeapAllocatorDXR(ID3D12DevicePtr device, ID3D12DescriptorHeapPtr heap);
    DescriptorHandleDXR allocate();

private:
    UINT m_stride{};
    D3D12_CPU_DESCRIPTOR_HANDLE m_hcpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_hgpu{};
};


class TextureDataDXR
{
public:
    void *texture = nullptr; // host
    int width = 0;
    int height = 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    ID3D12ResourcePtr  resource;
    ID3D11Texture2DPtr temporary_d3d11;
    HANDLE handle = nullptr;
    bool is_nt_handle = false;
    int use_count = 0;

    TextureDataDXR();
    ~TextureDataDXR();
};
using TextureDataDXRPtr = std::shared_ptr<TextureDataDXR>;

class BufferDataDXR
{
public:
    void *buffer = nullptr; // host
    int size = 0;

    ID3D12ResourcePtr resource;
    ID3D11BufferPtr   temporary_d3d11;
    HANDLE handle = nullptr;
    bool is_nt_handle = false;
    int use_count = 0;

    BufferDataDXR();
    ~BufferDataDXR();
};
using BufferDataDXRPtr = std::shared_ptr<BufferDataDXR>;

class MeshDataDXR
{
public:
    MeshData *base = nullptr;

    BufferDataDXRPtr vertex_buffer;
    BufferDataDXRPtr index_buffer;

    ID3D12ResourcePtr mesh_info;

    // blendshape data
    ID3D12ResourcePtr bs_delta;
    ID3D12ResourcePtr bs_frames;
    ID3D12ResourcePtr bs_info;

    // skinning data
    ID3D12ResourcePtr bone_counts;
    ID3D12ResourcePtr bone_weights;

    ID3D12ResourcePtr blas; // bottom level acceleration structure
    ID3D12ResourcePtr blas_scratch;

    int getVertexStride() const;
    int getIndexStride() const;
};
using MeshDataDXRPtr = std::shared_ptr<MeshDataDXR>;

class MeshInstanceDataDXR
{
public:
    MeshInstanceData *base = nullptr;

    MeshDataDXRPtr mesh;
    ID3D12DescriptorHeapPtr srvuav_heap;
    ID3D12ResourcePtr bs_weights;
    ID3D12ResourcePtr bone_matrices;
    ID3D12ResourcePtr deformed_vertices;
    ID3D12ResourcePtr blas_deformed;
    ID3D12ResourcePtr blas_scratch;
};
using MeshInstanceDataDXRPtr = std::shared_ptr<MeshInstanceDataDXR>;

class RenderDataDXR
{
public:
    ID3D12GraphicsCommandList4Ptr cmd_list_direct;
    ID3D12GraphicsCommandList4Ptr cmd_list_copy;

    ID3D12DescriptorHeapPtr desc_heap;
    DescriptorHandleDXR tlas_handle;
    DescriptorHandleDXR scene_data_handle;
    DescriptorHandleDXR render_target_handle;

    std::vector<MeshInstanceDataDXRPtr> mesh_instances;
    ID3D12ResourcePtr instance_desc;
    ID3D12ResourcePtr tlas_scratch;
    ID3D12ResourcePtr tlas;
    ID3D12ResourcePtr scene_data;
    TextureDataDXRPtr render_target;

    ID3D12ResourcePtr shader_table;

    uint64_t fence_value = 0;
    int render_flags = 0;
};

// thin wrapper for Windows' event
class FenceEventDXR
{
public:
    FenceEventDXR();
    ~FenceEventDXR();
    operator HANDLE() const;

private:
    HANDLE m_handle = nullptr;
};

class CommandManagerDXR
{
public:
    CommandManagerDXR(ID3D12DevicePtr device, ID3D12FencePtr fence);
    ID3D12CommandAllocatorPtr getAllocator(D3D12_COMMAND_LIST_TYPE type);
    ID3D12CommandQueuePtr getQueue(D3D12_COMMAND_LIST_TYPE type);
    ID3D12GraphicsCommandList4Ptr allocCommandList(D3D12_COMMAND_LIST_TYPE type);
    void releaseCommandList(ID3D12GraphicsCommandList4Ptr cl);
    uint64_t submit(ID3D12GraphicsCommandList4Ptr cl, ID3D12GraphicsCommandList4Ptr prev = nullptr, ID3D12GraphicsCommandList4Ptr next = nullptr);
    void wait(uint64_t fence_value);

    void resetQueues();
    void reset(ID3D12GraphicsCommandList4Ptr cl, ID3D12PipelineStatePtr state = nullptr);

    void setFenceValue(uint64_t v);
    uint64_t inclementFenceValue();

private:
    ID3D12DevicePtr m_device;
    ID3D12FencePtr m_fence;

    ID3D12CommandAllocatorPtr m_allocator_copy, m_allocator_direct, m_allocator_compute;
    ID3D12CommandQueuePtr m_queue_copy, m_queue_direct, m_queue_compute;
    std::vector<ID3D12GraphicsCommandList4Ptr> m_list_copy_pool, m_list_direct_pool, m_list_compute_pool;
    uint64_t m_fence_value = 0;
    FenceEventDXR m_fence_event;
};

class TimestampDXR
{
public:
    TimestampDXR(ID3D12DevicePtr device, int max_sample = 64);

    bool valid() const;
    void reset();
    bool query(ID3D12GraphicsCommandList4Ptr cl, const char *message);
    bool resolve(ID3D12GraphicsCommandList4Ptr cl);

    std::vector<std::tuple<uint64_t, const char*>> getSamples();
    void printElapsed(ID3D12CommandQueuePtr cq);

private:
    ID3D12QueryHeapPtr m_query_heap;
    ID3D12ResourcePtr m_timestamp_buffer;
    int m_max_sample = 0;
    int m_sample_index=0;
    std::vector<std::string> m_messages;
};
using TimestampDXRPtr = std::shared_ptr<TimestampDXR>;

#ifdef rthsEnableTimestamp
    #define TimestampInitialize(q, d) q = std::make_shared<TimestampDXR>(d)
    #define TimestampReset(q) q->reset()
    #define TimestampQuery(q, cl, m) q->query(cl, m)
    #define TimestampResolve(q, cl) q->resolve(cl)
    #define TimestampPrint(q, cq) q->printElapsed(cq)
#else rthsEnableTimestamp
    #define TimestampInitialize(...)
    #define TimestampReset(...)
    #define TimestampQuery(...)
    #define TimestampResolve(...)
    #define TimestampPrint(...)
#endif rthsEnableTimestamp

#ifdef rthsEnableResourceName
    #define DbgSetName(res, name) res->SetName(name)
#else
    #define DbgSetName(...)
#endif


extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;

DXGI_FORMAT GetTypedFormatDXR(DXGI_FORMAT format);
std::string ToString(ID3DBlob *blob);

} // namespace rths
#endif // _WIN32
