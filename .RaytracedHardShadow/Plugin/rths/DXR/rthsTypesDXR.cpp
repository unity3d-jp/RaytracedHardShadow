#include "pch.h"
#ifdef _WIN32
#include "Foundation/rthsLog.h"
#include "rthsTypesDXR.h"

namespace rths {

TextureDataDXR::TextureDataDXR()
{
}

TextureDataDXR::~TextureDataDXR()
{
    if (handle && is_nt_handle)
        ::CloseHandle(handle);
}

BufferDataDXR::BufferDataDXR()
{
}

BufferDataDXR::~BufferDataDXR()
{
    if (handle && is_nt_handle)
        ::CloseHandle(handle);
}




FenceEventDXR::FenceEventDXR()
{
    m_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

FenceEventDXR::~FenceEventDXR()
{
    ::CloseHandle(m_handle);
}
FenceEventDXR::operator HANDLE() const
{
    return m_handle;
}


int MeshDataDXR::getVertexStride() const
{
    if (base->vertex_stride == 0 && vertex_buffer)
        return vertex_buffer->size / base->vertex_count;
    else
        return base->vertex_stride;
}

int MeshDataDXR::getIndexStride() const
{
    if (base->index_stride == 0 && index_buffer)
        return index_buffer->size / base->index_count;
    else
        return base->index_stride;
}


DescriptorHandleDXR::operator bool() const
{
    return hcpu.ptr != 0 && hgpu.ptr != 0;
}

DescriptorHeapAllocatorDXR::DescriptorHeapAllocatorDXR(ID3D12DevicePtr device, ID3D12DescriptorHeapPtr heap)
{
    m_stride = device->GetDescriptorHandleIncrementSize(heap->GetDesc().Type);
    m_hcpu = heap->GetCPUDescriptorHandleForHeapStart();
    m_hgpu = heap->GetGPUDescriptorHandleForHeapStart();
}

DescriptorHandleDXR DescriptorHeapAllocatorDXR::allocate()
{
    DescriptorHandleDXR ret;
    ret.hcpu = m_hcpu;
    ret.hgpu = m_hgpu;
    m_hcpu.ptr += m_stride;
    m_hgpu.ptr += m_stride;
    return ret;
}


bool GeometryDataDXR::operator==(const GeometryDataDXR& v) const
{
    return inst == v.inst && hit_mask == v.hit_mask;
}

bool GeometryDataDXR::operator!=(const GeometryDataDXR& v) const
{
    return !(*this == v);
}



CommandManagerDXR::CommandManagerDXR(ID3D12DevicePtr device, ID3D12FencePtr fence)
    : m_device(device)
    , m_fence(fence)
{
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue_copy));
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_allocator_copy));
    }
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue_direct));
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocator_direct));
    }
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_queue_compute));
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_allocator_compute));
    }
}

ID3D12CommandAllocatorPtr CommandManagerDXR::getAllocator(D3D12_COMMAND_LIST_TYPE type)
{
    switch (type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        return m_allocator_direct;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        return m_allocator_compute;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        return m_allocator_copy;
    default:
        return nullptr;
    }
}

ID3D12CommandQueuePtr CommandManagerDXR::getQueue(D3D12_COMMAND_LIST_TYPE type)
{
    switch (type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        return m_queue_direct;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        return m_queue_compute;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        return m_queue_copy;
    default:
        return nullptr;
    }
}

ID3D12GraphicsCommandList4Ptr CommandManagerDXR::allocCommandList(D3D12_COMMAND_LIST_TYPE type)
{
    ID3D12GraphicsCommandList4Ptr ret;
    m_device->CreateCommandList(0, type, getAllocator(type), nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

void CommandManagerDXR::releaseCommandList(ID3D12GraphicsCommandList4Ptr cl)
{
    auto type = cl->GetType();
    switch (type) {
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        m_list_direct_pool.push_back(cl);
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        m_list_compute_pool.push_back(cl);
        break;
    case D3D12_COMMAND_LIST_TYPE_COPY:
        m_list_copy_pool.push_back(cl);
        break;
    default:
        break;
    }
}

uint64_t CommandManagerDXR::submit(ID3D12GraphicsCommandList4Ptr cl, ID3D12GraphicsCommandList4Ptr prev, ID3D12GraphicsCommandList4Ptr next)
{
    uint64_t fence_value = m_fence_value;
    auto type = cl->GetType();
    auto queue = getQueue(type);

    if (prev) {
        auto type_prev = prev->GetType();
        auto queue_prev = getQueue(type_prev);
        queue_prev->Signal(m_fence, fence_value);
        queue->Wait(m_fence, fence_value);
        ++fence_value;
    }
    {
        cl->Close();
        ID3D12CommandList* cmd_list[]{ cl.GetInterfacePtr() };
        queue->ExecuteCommandLists(_countof(cmd_list), cmd_list);
    }
    if (next) {
        auto type_next = next->GetType();
        auto queue_next = getQueue(type_next);

        queue->Signal(m_fence, fence_value);
        queue_next->Wait(m_fence, fence_value);
        ++fence_value;
    }
    m_fence_value = fence_value;
    return fence_value;
}

void CommandManagerDXR::resetQueues()
{
    m_allocator_copy->Reset();
    m_allocator_direct->Reset();
    m_allocator_compute->Reset();
}

void CommandManagerDXR::reset(ID3D12GraphicsCommandList4Ptr cl, ID3D12PipelineStatePtr state)
{
    cl->Reset(getAllocator(cl->GetType()), state);
}

void CommandManagerDXR::wait(uint64_t fence_value)
{
    m_fence->SetEventOnCompletion(fence_value, m_fence_event);
    ::WaitForSingleObject(m_fence_event, INFINITE);
}

void CommandManagerDXR::setFenceValue(uint64_t v)
{
    m_fence_value = v;
}

uint64_t CommandManagerDXR::inclementFenceValue()
{
    return ++m_fence_value;
}


TimestampDXR::TimestampDXR(ID3D12DevicePtr device, int max_sample)
{
    m_max_sample = max_sample;
    if (!device)
        return;

    m_messages.resize(m_max_sample);
    {
        D3D12_QUERY_HEAP_DESC  desc{};
        desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        desc.Count = m_max_sample;
        device->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_query_heap));
    }
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(uint64_t) * m_max_sample;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        device->CreateCommittedResource(&kReadbackHeapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_timestamp_buffer));
    }
}

bool TimestampDXR::valid() const
{
    return m_query_heap && m_timestamp_buffer;
}

void TimestampDXR::reset()
{
    m_sample_index = 0;
}

bool TimestampDXR::query(ID3D12GraphicsCommandList4Ptr cl, const char *message)
{
    if (!valid() || m_sample_index == m_max_sample)
        return false;
    int si = m_sample_index++;
    cl->EndQuery(m_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, si);
    m_messages[si] = message;
    return true;
}

bool TimestampDXR::resolve(ID3D12GraphicsCommandList4Ptr cl)
{
    if (!valid())
        return false;
    cl->ResolveQueryData(m_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, m_sample_index, m_timestamp_buffer, 0);
    return true;
}

std::vector<std::tuple<uint64_t, std::string*>> TimestampDXR::getSamples()
{
    std::vector<std::tuple<uint64_t, std::string*>> ret;
    if (!valid())
        return ret;

    ret.resize(m_sample_index);

    uint64_t *data;
    D3D12_RANGE ragne{ 0, sizeof(uint64_t)*m_sample_index };
    auto hr = m_timestamp_buffer->Map(0, &ragne, (void**)&data);
    if (SUCCEEDED(hr)) {
        for (int si = 0; si < m_sample_index; ++si)
            ret[si] = std::make_tuple(data[si], &m_messages[si]);
        m_timestamp_buffer->Unmap(0, nullptr);
    }
    return ret;
}

void TimestampDXR::printElapsed(ID3D12CommandQueuePtr cq)
{
    if (!valid() || !cq)
        return;

    auto time_samples = getSamples();
    if (!time_samples.empty()) {
        uint64_t freq = 0;
        cq->GetTimestampFrequency(&freq);

        size_t n = time_samples.size();
        auto base = time_samples[0];
        for (size_t si = 0; si < n; ++si) {
            auto s1 = time_samples[si];
            auto pos1 = std::get<1>(s1)->find(" begin");
            if (pos1 != std::string::npos) {
                auto it = std::find_if(time_samples.begin() + (si + 1), time_samples.end(),
                    [&](auto& s2) {
                        auto pos2 = std::get<1>(s2)->find(" end");
                        if (pos2 != std::string::npos) {
                            return pos1 == pos2 &&
                                std::strncmp(std::get<1>(s1)->c_str(), std::get<1>(s2)->c_str(), pos1) == 0;
                        }
                        return false;
                    });
                if (it != time_samples.end()) {
                    auto& s2 = *it;
                    auto epalsed = std::get<0>(s2) - std::get<0>(s1);
                    auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
                    DebugPrint("%s %f\n", std::get<1>(s2)->c_str(), epalsed_ms);
                    continue;
                }
            }

            auto end_pos = std::get<1>(s1)->find(" end");
            if (pos1 == std::string::npos && end_pos == std::string::npos) {
                auto epalsed = std::get<0>(s1);
                auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
                DebugPrint("%s %f\n", std::get<1>(s1)->c_str(), epalsed_ms);
            }
        }
    }
}



DXGI_FORMAT GetTypedFormatDXR(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8_TYPELESS:
        return DXGI_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8G8_TYPELESS:
        return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_R16_TYPELESS:
        return DXGI_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16G16_TYPELESS:
        return DXGI_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case DXGI_FORMAT_R32_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS:
        return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    default:
        return format;
    }
}

std::string ToString(ID3DBlob *blob)
{
    std::string ret;
    ret.resize(blob->GetBufferSize());
    memcpy(&ret[0], blob->GetBufferPointer(), blob->GetBufferSize());
    return ret;
}

} // namespace rths
#endif // _WIN32
