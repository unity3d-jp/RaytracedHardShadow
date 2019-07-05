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
    return inst == v.inst && receive_mask == v.receive_mask && cast_mask == v.cast_mask;
}

bool GeometryDataDXR::operator!=(const GeometryDataDXR& v) const
{
    return !(*this == v);
}



CommandListManagerDXR::Record::Record(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state)
{
    device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator));
    device->CreateCommandList(0, type, allocator, state, IID_PPV_ARGS(&list));
}

void CommandListManagerDXR::Record::reset(ID3D12PipelineStatePtr state)
{
    allocator->Reset();
    list->Reset(allocator, state);
}

CommandListManagerDXR::CommandListManagerDXR(ID3D12DevicePtr device, D3D12_COMMAND_LIST_TYPE type, ID3D12PipelineStatePtr state)
    : m_device(device)
    , m_type(type)
    , m_state(state)
{
}

ID3D12GraphicsCommandList4Ptr CommandListManagerDXR::get()
{
    ID3D12GraphicsCommandList4Ptr ret;
    if (!m_available.empty()) {
        auto c = m_available.back();
        m_in_use.push_back(c);
        m_available.pop_back();
        ret = c->list;
    }
    else
    {
        auto c = std::make_shared<Record>(m_device, m_type, m_state);
        m_in_use.push_back(c);
        ret = c->list;
    }
    m_raw.push_back(ret);
    return ret;
}

void CommandListManagerDXR::reset()
{
    for(auto& p : m_in_use)
        p->reset(m_state);
    m_available.insert(m_available.end(), m_in_use.begin(), m_in_use.end());
    m_in_use.clear();
    m_raw.clear();
}

const std::vector<ID3D12CommandList*>& CommandListManagerDXR::getCommandLists() const
{
    return m_raw;
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

bool TimestampDXR::isEnabled() const
{
    return m_enabled;
}

void TimestampDXR::setEnabled(bool v)
{
    m_enabled = v;
}

void TimestampDXR::reset()
{
    m_sample_index = 0;
}

bool TimestampDXR::query(ID3D12GraphicsCommandList4Ptr cl, const char *message)
{
    if (!valid() || !m_enabled || m_sample_index == m_max_sample)
        return false;
    int si = m_sample_index++;
    cl->EndQuery(m_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, si);
    m_messages[si] = message;
    return true;
}

bool TimestampDXR::resolve(ID3D12GraphicsCommandList4Ptr cl)
{
    if (!valid() || !m_enabled)
        return false;
    cl->ResolveQueryData(m_query_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, m_sample_index, m_timestamp_buffer, 0);
    return true;
}

std::vector<std::tuple<uint64_t, std::string*>> TimestampDXR::getSamples()
{
    std::vector<std::tuple<uint64_t, std::string*>> ret;
    if (!valid() || !m_enabled)
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

void TimestampDXR::updateLog(ID3D12CommandQueuePtr cq)
{
    if (!valid() || !m_enabled || !cq)
        return;

    m_log.clear();
    char name[256];
    char buf[256];
    auto time_samples = getSamples();
    if (!time_samples.empty()) {
        uint64_t freq = 0;
        cq->GetTimestampFrequency(&freq);

        size_t n = time_samples.size();
        auto base = time_samples[0];
        for (size_t si = 0; si < n; ++si) {
            auto s1 = time_samples[si];
            auto name1 = std::get<1>(s1);
            auto pos1 = name1->find(" begin");
            if (pos1 != std::string::npos) {
                auto it = std::find_if(time_samples.begin() + (si + 1), time_samples.end(),
                    [&](auto& s2) {
                        auto name2 = std::get<1>(s2);
                        auto pos2 = name2->find(" end");
                        if (pos2 != std::string::npos) {
                            return pos1 == pos2 &&
                                std::strncmp(name1->c_str(), name2->c_str(), pos1) == 0;
                        }
                        return false;
                    });
                if (it != time_samples.end()) {
                    auto& s2 = *it;
                    std::strncpy(name, name1->c_str(), pos1);
                    name[pos1] = '\0';
                    auto epalsed = std::get<0>(s2) - std::get<0>(s1);
                    auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
                    sprintf(buf, "%s: %.2fms\n", name, epalsed_ms);
                    m_log += buf;
                    continue;
                }
            }

            auto end_pos = name1->find(" end");
            if (pos1 == std::string::npos && end_pos == std::string::npos) {
                auto epalsed = std::get<0>(s1);
                auto epalsed_ms = float(double(epalsed * 1000) / double(freq));
                sprintf(buf, "%s: %.2fms\n", name1->c_str(), epalsed_ms);
                m_log += buf;
            }
        }
    }
}

const std::string & TimestampDXR::getLog() const
{
    return m_log;
}


size_t SizeOfElement(DXGI_FORMAT rtf)
{
    switch (rtf) {
    case DXGI_FORMAT_R8_TYPELESS: return 1;
    case DXGI_FORMAT_R8G8_TYPELESS: return 2;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return 4;

    case DXGI_FORMAT_R16_TYPELESS: return 2;
    case DXGI_FORMAT_R16G16_TYPELESS: return 4;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return 8;

    case DXGI_FORMAT_R32_TYPELESS: return 4;
    case DXGI_FORMAT_R32G32_TYPELESS: return 8;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return 16;
    }
    return 0;
}

DXGI_FORMAT GetDXGIFormat(RenderTargetFormat format)
{
    switch (format) {
    case RenderTargetFormat::Ru8: return DXGI_FORMAT_R8_TYPELESS;
    case RenderTargetFormat::RGu8: return DXGI_FORMAT_R8G8_TYPELESS;
    case RenderTargetFormat::RGBAu8: return DXGI_FORMAT_R8G8B8A8_TYPELESS;

    case RenderTargetFormat::Rf16: return DXGI_FORMAT_R16_TYPELESS;
    case RenderTargetFormat::RGf16: return DXGI_FORMAT_R16G16_TYPELESS;
    case RenderTargetFormat::RGBAf16: return DXGI_FORMAT_R16G16B16A16_TYPELESS;

    case RenderTargetFormat::Rf32: return DXGI_FORMAT_R32_TYPELESS;
    case RenderTargetFormat::RGf32: return DXGI_FORMAT_R32G32_TYPELESS;
    case RenderTargetFormat::RGBAf32: return DXGI_FORMAT_R32G32B32A32_TYPELESS;

    default: return DXGI_FORMAT_UNKNOWN;
    }
}

DXGI_FORMAT GetTypedFormatDXR(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8_TYPELESS: return DXGI_FORMAT_R8_UNORM;
    case DXGI_FORMAT_R8G8_TYPELESS: return DXGI_FORMAT_R8G8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;

    case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R16G16_TYPELESS: return DXGI_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;

    case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS: return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;

    default: return format;
    }
}

std::string ToString(ID3DBlob *blob)
{
    std::string ret;
    ret.resize(blob->GetBufferSize());
    memcpy(&ret[0], blob->GetBufferPointer(), blob->GetBufferSize());
    return ret;
}

bool RenderDataDXR::hasFlag(RenderFlag f) const
{
    return (render_flags & (uint32_t)f) != 0;
}

} // namespace rths
#endif // _WIN32
