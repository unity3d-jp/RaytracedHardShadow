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

    int use_count = 0;

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
};
using MeshInstanceDataDXRPtr = std::shared_ptr<MeshInstanceDataDXR>;


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


extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;

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

DXGI_FORMAT GetTypedFormatDXR(DXGI_FORMAT format);
std::string ToString(ID3DBlob *blob);

} // namespace rths
#endif // _WIN32
