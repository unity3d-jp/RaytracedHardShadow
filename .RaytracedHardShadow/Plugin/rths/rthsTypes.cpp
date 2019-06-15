#include "pch.h"
#include "rthsTypes.h"

namespace rths {

bool MeshData::operator==(const MeshData& v) const
{
    return std::memcmp(this, &v, sizeof(*this)) == 0;
}

bool MeshData::operator<(const MeshData& v) const
{
    return std::memcmp(this, &v, sizeof(*this)) < 0;
}

bool SkinData::operator==(const SkinData& v) const
{
    return std::memcmp(this, &v, sizeof(*this)) == 0;
}
bool SkinData::operator<(const SkinData& v) const
{
    return std::memcmp(this, &v, sizeof(*this)) < 0;
}

} // namespace rths 
