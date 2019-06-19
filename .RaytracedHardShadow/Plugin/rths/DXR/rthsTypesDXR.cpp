#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
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

std::vector<std::tuple<uint64_t, const char*>> TimestampDXR::getSamples()
{
    std::vector<std::tuple<uint64_t, const char*>> ret;
    if (!valid())
        return ret;

    ret.resize(m_sample_index);

    uint64_t *data;
    D3D12_RANGE ragne{ 0, sizeof(uint64_t)*m_sample_index };
    auto hr = m_timestamp_buffer->Map(0, &ragne, (void**)&data);
    if (SUCCEEDED(hr)) {
        for (int si = 0; si < m_sample_index; ++si)
            ret[si] = std::make_tuple(data[si], m_messages[si].c_str());
        m_timestamp_buffer->Unmap(0, nullptr);
    }
    return ret;
}

void TimestampDXR::printElapsed(ID3D12CommandQueuePtr cq)
{
    auto time_samples = getSamples();
    if (!time_samples.empty()) {
        uint64_t freq = 0;
        cq->GetTimestampFrequency(&freq);

        size_t n = time_samples.size();
        auto base = time_samples[0];
        for (size_t si = 1; si < n; ++si) {
            auto sample = time_samples[si];
            auto message = std::get<1>(sample);
            auto epalsed = std::get<0>(sample) - std::get<0>(base);
            auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
            DebugPrint("%s %f\n", message, epalsed_ms);
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
