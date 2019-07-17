#pragma once
#ifdef _WIN32
#include "rthsTypes.h"

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

// thin wrapper for Windows' event
class FenceEventDXR
{
public:
    FenceEventDXR();
    FenceEventDXR(const FenceEventDXR& v);
    FenceEventDXR& operator=(const FenceEventDXR& v);
    ~FenceEventDXR();
    operator HANDLE() const;

private:
    HANDLE m_handle = nullptr;
};


class TextureDataDXR
{
public:
    GPUResourcePtr host_ptr = nullptr;
    int width = 0;
    int height = 0;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    ID3D12ResourcePtr  resource, host_d3d12;
    ID3D11Texture2DPtr temporary_d3d11, host_d3d11;
    HANDLE handle = nullptr;
    bool is_nt_handle = false;
    ULONG initial_ref = 0;

    TextureDataDXR();
    ~TextureDataDXR();
};
using TextureDataDXRPtr = std::shared_ptr<TextureDataDXR>;

class BufferDataDXR
{
public:
    GPUResourcePtr host_ptr = nullptr;
    int size = 0;

    ID3D12ResourcePtr resource, host_d3d12;
    ID3D11BufferPtr   temporary_d3d11, host_d3d11;
    HANDLE handle = nullptr;
    bool is_nt_handle = false;
    ULONG initial_ref = 0;

    BufferDataDXR();
    ~BufferDataDXR();
};
using BufferDataDXRPtr = std::shared_ptr<BufferDataDXR>;

class MeshDataDXR : public DeviceMeshData
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

    bool valid() const override;
    bool isRelocated() const override;
    int getVertexStride() const;
    int getIndexStride() const;
    void clearBLAS();
};
using MeshDataDXRPtr = std::shared_ptr<MeshDataDXR>;

class MeshInstanceDataDXR : public DeviceMeshInstanceData
{
public:
    MeshInstanceData *base = nullptr;

    MeshDataDXRPtr mesh;
    ID3D12DescriptorHeapPtr desc_heap;
    ID3D12ResourcePtr bs_weights;
    ID3D12ResourcePtr bone_matrices;
    ID3D12ResourcePtr deformed_vertices;
    ID3D12ResourcePtr blas_deformed;
    ID3D12ResourcePtr blas_scratch;
    bool is_updated = false;

    bool valid() const override;
    void clearBLAS();
};
using MeshInstanceDataDXRPtr = std::shared_ptr<MeshInstanceDataDXR>;

class RenderTargetDataDXR : public DeviceRenderTargetData
{
public:
    RenderTargetData *base = nullptr;
    TextureDataDXRPtr texture;
    ID3D12ResourcePtr adaptive_res[3]; // for adaptive sampling
    ID3D12ResourcePtr back_buffer; // back buffer for antialiasing

    bool valid() const override;
    bool isRelocated() const override;
};
using RenderTargetDataDXRPtr = std::shared_ptr<RenderTargetDataDXR>;


class TimestampDXR
{
public:
    TimestampDXR(ID3D12DevicePtr device, int max_sample = 64);

    bool valid() const;
    bool isEnabled() const;
    void setEnabled(bool v);
    void reset();
    bool query(ID3D12GraphicsCommandList4Ptr cl, const char *message);
    bool resolve(ID3D12GraphicsCommandList4Ptr cl);

    std::vector<std::tuple<uint64_t, std::string*>> getSamples();
    void updateLog(ID3D12CommandQueuePtr cq);
    const std::string& getLog() const;

private:
    ID3D12QueryHeapPtr m_query_heap;
    ID3D12ResourcePtr m_timestamp_buffer;
    bool m_enabled = true;
    int m_max_sample = 0;
    int m_sample_index=0;
    std::vector<std::string> m_messages;
    std::string m_log;
};
using TimestampDXRPtr = std::shared_ptr<TimestampDXR>;

#ifdef rthsEnableTimestamp
    #define rthsTimestampInitialize(q, d)   if (!q) { q = std::make_shared<TimestampDXR>(d); }
    #define rthsTimestampSetEnable(q, e)    q->setEnabled(e)
    #define rthsTimestampReset(q)           q->reset()
    #define rthsTimestampQuery(q, cl, m)    q->query(cl, m)
    #define rthsTimestampResolve(q, cl)     q->resolve(cl)
    #define rthsTimestampUpdateLog(q, cq)   q->updateLog(cq)
#else rthsEnableTimestamp
    #define rthsTimestampInitialize(...)
    #define rthsTimestampSetEnable(...)
    #define rthsTimestampReset(...)
    #define rthsTimestampQuery(...)
    #define rthsTimestampResolve(...)
    #define rthsTimestampUpdateLog(...)
#endif rthsEnableTimestamp

void SetNameImpl(ID3D12Object *obj, LPCSTR name);
void SetNameImpl(ID3D12Object *obj, LPCWSTR name);
void SetNameImpl(ID3D12Object *obj, const std::string& name);
void SetNameImpl(ID3D12Object *obj, const std::wstring& name);
#ifdef rthsEnableResourceName
    #define rthsSetName(res, name) SetNameImpl(res, name)
#else
    #define rthsSetName(...)
#endif

class CommandListManagerDXR
{
public:
    CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, const wchar_t *name);
    CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state, const wchar_t *name);
    ID3D12GraphicsCommandList4Ptr get();
    void reset();

    // command lists to pass ExecuteCommandLists()
    const std::vector<ID3D12CommandList*>& getCommandLists() const;

private:
    struct Record
    {
        Record(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state);
        void reset(ID3D12PipelineStatePtr state);

        ID3D12CommandAllocatorPtr allocator;
        ID3D12GraphicsCommandList4Ptr list;
    };
    using CommandPtr = std::shared_ptr<Record>;

    ID3D12DevicePtr m_device;
    D3D12_COMMAND_LIST_TYPE m_type;
    ID3D12PipelineStatePtr m_state;
    std::vector<CommandPtr> m_available, m_in_use;
    std::vector<ID3D12CommandList*> m_raw;
    std::wstring m_name;
};
using CommandListManagerDXRPtr = std::shared_ptr<CommandListManagerDXR>;

const int kMaxTLASCount = 4;

class SceneDataDXR
{
    SceneData base;
    int active_tlas_count;
};

class TLASDataDXR
{
public:
    ID3D12ResourcePtr instance_desc;
    ID3D12ResourcePtr scratch;
    ID3D12ResourcePtr buffer;
    DescriptorHandleDXR srv;
    uint32_t nth = 0;
    uint32_t instance_offset = 0;
};

class RenderDataDXR
{
public:
    std::string name;

    ID3D12GraphicsCommandList4Ptr cl_deform;
    ID3D12DescriptorHeapPtr desc_heap;
    DescriptorHandleDXR render_target_uav;
    DescriptorHandleDXR instance_data_srv;
    DescriptorHandleDXR scene_data_cbv;
    DescriptorHandleDXR adaptive_uavs[3], adaptive_srvs[3];
    DescriptorHandleDXR back_buffer_uav, back_buffer_srv;

    std::vector<MeshInstanceDataDXRPtr> instances, instances_prev;
    SceneData scene_data_prev{};
    TLASDataDXR tlas_data[kMaxTLASCount];
    ID3D12ResourcePtr scene_data;
    ID3D12ResourcePtr instance_data;
    RenderTargetDataDXRPtr render_target;

    uint64_t fv_translate = 0, fv_deform = 0, fv_blas = 0, fv_tlas = 0, fv_rays = 0;
    FenceEventDXR fence_event;
    uint32_t render_flags = 0;

#ifdef rthsEnableTimestamp
    TimestampDXRPtr timestamp;
#endif // rthsEnableTimestamp

    bool hasFlag(RenderFlag f) const;
    void clear();
};


extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;
static const DWORD kTimeoutMS = 3000;

ULONG GetRefCount(IUnknown *v);
UINT SizeOfElement(DXGI_FORMAT rtf);
DXGI_FORMAT GetDXGIFormat(RenderTargetFormat format);
DXGI_FORMAT GetTypedFormatDXR(DXGI_FORMAT format);
DXGI_FORMAT GetTypelessFormatDXR(DXGI_FORMAT format);
std::string ToString(ID3DBlob *blob);
void PrintStateObjectDesc(const D3D12_STATE_OBJECT_DESC* desc);

// Body : [](size_t size) -> ID3D12Resource
// return true if expanded
template<class Body>
bool ReuseOrExpandBuffer(ID3D12ResourcePtr &buf, size_t stride, size_t size, size_t minimum, const Body& body)
{
    if (buf) {
        auto capacity_in_byte = buf->GetDesc().Width;
        if (capacity_in_byte < size * stride)
            buf = nullptr;
    }
    if (!buf) {
        size_t capacity = minimum;
        while (capacity < size)
            capacity *= 2;
        buf = body(stride * capacity);
        return true;
    }
    return false;
};

} // namespace rths
#endif // _WIN32
