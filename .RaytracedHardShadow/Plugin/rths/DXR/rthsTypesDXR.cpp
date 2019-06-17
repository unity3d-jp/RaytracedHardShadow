#include "pch.h"
#ifdef _WIN32
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
