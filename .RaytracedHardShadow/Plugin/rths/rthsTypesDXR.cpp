#include "pch.h"
#ifdef _WIN32
#include "rthsTypesDXR.h"

namespace rths {

TextureID identifier(const TextureDataDXR& data)
{
    TextureID ret;
    ret.texture = (uint64_t)data.texture;
    ret.width = (uint32_t)data.width;
    ret.height = (uint32_t)data.height;
    return ret;
}

MeshID identifier(const MeshDataDXR& data)
{
    MeshID ret;
    ret.vertex_buffer = (uint64_t)data.vertex_buffer.buffer;
    ret.index_buffer = (uint64_t)data.index_buffer.buffer;
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
} // namespace rths
#endif // _WIN32
