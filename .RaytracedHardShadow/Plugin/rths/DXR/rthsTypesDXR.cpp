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

TextureID identifier(const TextureDataDXR& data)
{
    TextureID ret;
    ret.texture = (uint64_t)data.texture;
    ret.width = (uint32_t)data.width;
    ret.height = (uint32_t)data.height;
    return ret;
}

BufferDataDXR::BufferDataDXR()
{
}

BufferDataDXR::~BufferDataDXR()
{
    if (handle && is_nt_handle)
        ::CloseHandle(handle);
}



MeshID identifier(const MeshDataDXR& data)
{
    MeshID ret;
    ret.vertex_buffer = (uint64_t)data.vertex_buffer->buffer;
    ret.index_buffer = (uint64_t)data.index_buffer->buffer;
    ret.vertex_count = (uint32_t)data.vertex_count;
    ret.index_count = (uint32_t)data.index_count;
    ret.index_offset = (uint32_t)data.index_offset;
    return ret;
}


DescriptorHandle::operator bool() const
{
    return hcpu.ptr != 0 && hgpu.ptr != 0;
}


FenceEvent::FenceEvent()
{
    m_handle = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

FenceEvent::~FenceEvent()
{
    ::CloseHandle(m_handle);
}
FenceEvent::operator HANDLE() const
{
    return m_handle;
}


DXGI_FORMAT GetTypedFormat(DXGI_FORMAT format)
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

} // namespace rths
#endif // _WIN32
