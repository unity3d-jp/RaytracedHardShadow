#include "pch.h"
#include "rthsTypes.h"

namespace rths {

float4x4 operator*(const float4x4 &a, const float4x4 &b)
{
    float4x4 c;
    const float *ap = &a[0][0];
    const float *bp = &b[0][0];
    float *cp = &c[0][0];
    float a0, a1, a2, a3;

    a0 = ap[0];
    a1 = ap[1];
    a2 = ap[2];
    a3 = ap[3];

    cp[0] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[1] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[2] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[3] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

    a0 = ap[4];
    a1 = ap[5];
    a2 = ap[6];
    a3 = ap[7];

    cp[4] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[5] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[6] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[7] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

    a0 = ap[8];
    a1 = ap[9];
    a2 = ap[10];
    a3 = ap[11];

    cp[8] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[9] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[10] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[11] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];

    a0 = ap[12];
    a1 = ap[13];
    a2 = ap[14];
    a3 = ap[15];

    cp[12] = a0 * bp[0] + a1 * bp[4] + a2 * bp[8] + a3 * bp[12];
    cp[13] = a0 * bp[1] + a1 * bp[5] + a2 * bp[9] + a3 * bp[13];
    cp[14] = a0 * bp[2] + a1 * bp[6] + a2 * bp[10] + a3 * bp[14];
    cp[15] = a0 * bp[3] + a1 * bp[7] + a2 * bp[11] + a3 * bp[15];
    return c;
}

float4x4 invert(const float4x4& x)
{
    float4x4 s{
        x[1][1] * x[2][2] - x[2][1] * x[1][2],
        x[2][1] * x[0][2] - x[0][1] * x[2][2],
        x[0][1] * x[1][2] - x[1][1] * x[0][2],
        0,

        x[2][0] * x[1][2] - x[1][0] * x[2][2],
        x[0][0] * x[2][2] - x[2][0] * x[0][2],
        x[1][0] * x[0][2] - x[0][0] * x[1][2],
        0,

        x[1][0] * x[2][1] - x[2][0] * x[1][1],
        x[2][0] * x[0][1] - x[0][0] * x[2][1],
        x[0][0] * x[1][1] - x[1][0] * x[0][1],
        0,

        0, 0, 0, 1,
    };

    auto r = x[0][0] * s[0][0] + x[0][1] * s[1][0] + x[0][2] * s[2][0];

    if (std::abs(r) >= 1) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                s[i][j] /= r;
            }
        }
    }
    else {
        auto mr = std::abs(r) / std::numeric_limits<float>::min();

        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (mr > std::abs(s[i][j])) {
                    s[i][j] /= r;
                }
                else {
                    // error
                    return float4x4::identity();
                }
            }
        }
    }

    s[3][0] = -x[3][0] * s[0][0] - x[3][1] * s[1][0] - x[3][2] * s[2][0];
    s[3][1] = -x[3][0] * s[0][1] - x[3][1] * s[1][1] - x[3][2] * s[2][1];
    s[3][2] = -x[3][0] * s[0][2] - x[3][1] * s[1][2] - x[3][2] * s[2][2];
    return s;
}


bool SkinData::valid() const
{
    return !bindposes.empty() && !bone_counts.empty() && !weights.empty();
}

void CallOnMeshDelete(MeshData *mesh);
void CallOnMeshInstanceDelete(MeshInstanceData *inst);

static uint64_t g_meshdata_id = 0;

MeshData::MeshData()
{
    id = ++g_meshdata_id;
}

MeshData::~MeshData()
{
    CallOnMeshDelete(this);
}

void MeshData::addref()
{
    ++ref_count;
}

void MeshData::release()
{
    if (--ref_count == 0)
        delete this;
}

bool MeshData::valid() const
{
    return vertex_buffer != nullptr && index_buffer != nullptr;
}

bool MeshData::operator==(const MeshData & v) const
{
    return id == v.id;
}
bool MeshData::operator!=(const MeshData & v) const
{
    return id != v.id;
}
bool MeshData::operator<(const MeshData& v) const
{
    return id < v.id;
}


static uint64_t g_meshinstancedata_id = 0;

MeshInstanceData::MeshInstanceData()
{
    id = ++g_meshinstancedata_id;
}

MeshInstanceData::~MeshInstanceData()
{
    CallOnMeshInstanceDelete(this);
}

void MeshInstanceData::addref()
{
    ++ref_count;
}

void MeshInstanceData::release()
{
    if (--ref_count == 0)
        delete this;
}

bool MeshInstanceData::valid() const
{
    return mesh && mesh->valid();
}

bool MeshInstanceData::operator==(const MeshInstanceData & v) const
{
    return id == v.id;
}
bool MeshInstanceData::operator!=(const MeshInstanceData & v) const
{
    return id != v.id;
}
bool MeshInstanceData::operator<(const MeshInstanceData & v) const
{
    return id < v.id;
}

bool GeometryData::valid() const
{
    return hit_mask != 0 && instance && instance->valid();
}

} // namespace rths 
