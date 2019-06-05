#include "pch.h"
#include "rthsTypes.h"

namespace rths {

bool MeshID::operator==(const MeshID& v) const
{
    return std::memcmp(this, &v, sizeof(*this)) == 0;
}

bool MeshID::operator<(const MeshID& v) const
{
    return std::memcmp(this, &v, sizeof(*this)) < 0;
}

MeshID identifier(const MeshData& data)
{
    MeshID ret;
    ret.vertex_buffer = (uint64_t)data.vertex_buffer;
    ret.index_buffer = (uint64_t)data.index_buffer;
    ret.vertex_count = (uint32_t)data.vertex_count;
    ret.index_count = (uint32_t)data.index_count;
    ret.index_offset = (uint32_t)data.index_offset;
    return ret;
}

} // namespace rths 
