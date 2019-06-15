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

void SkinDataHolder::assign(const SkinData & v)
{
    bone_counts.assign(v.bone_counts, v.bone_counts + v.num_bone_counts);
    weights1.assign(v.weights1, v.weights1 + v.num_weights1);
    weights4.assign(v.weights4, v.weights4 + v.num_weights4);
    matrices.assign(v.matrices, v.matrices + v.num_matrices);
}

} // namespace rths 
