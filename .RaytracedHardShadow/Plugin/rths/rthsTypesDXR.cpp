#include "pch.h"
#ifdef _WIN32
#include "rthsTypesDXR.h"

namespace rths {

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

} // namespace rths
#endif // _WIN32
