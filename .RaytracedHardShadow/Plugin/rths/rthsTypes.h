#pragma once

namespace rths {

constexpr float PI = 3.14159265358979323846264338327950288419716939937510f;
constexpr float DegToRad = PI / 180.0f;
constexpr float RadToDeg = 1.0f / (PI / 180.0f);

using nanosec = uint64_t;

struct int2
{
    int x, y;

    int& operator[](size_t i) { return ((int*)this)[i]; }
    const int& operator[](size_t i) const { return ((int*)this)[i]; }
};

struct int3
{
    int x, y, z;

    int& operator[](size_t i) { return ((int*)this)[i]; }
    const int& operator[](size_t i) const { return ((int*)this)[i]; }
};

struct int4
{
    int x, y, z, w;

    int& operator[](size_t i) { return ((int*)this)[i]; }
    const int& operator[](size_t i) const { return ((int*)this)[i]; }
};

struct float2
{
    float x,y;

    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float3
{
    float x,y,z;

    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float4
{
    float x,y,z,w;

    float& operator[](size_t i) { return ((float*)this)[i]; }
    const float& operator[](size_t i) const { return ((float*)this)[i]; }
};

struct float3x4
{
    float4 v[3];

    float4& operator[](size_t i) { return v[i]; }
    const float4& operator[](size_t i) const { return v[i]; }
};

struct float4x4
{
    float4 v[4];

    float4& operator[](size_t i) { return v[i]; }
    const float4& operator[](size_t i) const { return v[i]; }
    bool operator==(float4x4& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }
    bool operator!=(float4x4& v) const { return !(*this == v); }

    static float4x4 identity()
    {
        return{ {
             { 1, 0, 0, 0 },
             { 0, 1, 0, 0 },
             { 0, 0, 1, 0 },
             { 0, 0, 0, 1 },
         } };
    }
};


inline float3 operator-(const float3& l) { return{ -l.x, -l.y, -l.z }; }
inline float3 operator*(const float3& l, float r) { return{ l.x * r, l.y * r, l.z * r }; }
inline float3 operator/(const float3& l, float r) { return{ l.x / r, l.y / r, l.z / r }; }

inline float clamp(float v, float vmin, float vmax) { return std::min<float>(std::max<float>(v, vmin), vmax); }
inline float dot(const float3& l, const float3& r) { return l.x*r.x + l.y*r.y + l.z*r.z; }
inline float length_sq(const float3& v) { return dot(v, v); }
inline float length(const float3& v) { return sqrt(length_sq(v)); }
inline float3 normalize(const float3& v) { return v / length(v); }

inline float4 to_float4(const float3& xyz, float w)
{
    return{ xyz.x, xyz.y, xyz.z, w };
}

inline float3x4 to_float3x4(const float4x4& v)
{
    // copy with transpose
    return float3x4{ {
        {v[0][0], v[1][0], v[2][0], v[3][0]},
        {v[0][1], v[1][1], v[2][1], v[3][1]},
        {v[0][2], v[1][2], v[2][2], v[3][2]},
    } };
}
inline float3 extract_position(const float4x4& m)
{
    return (const float3&)m[3];
}
inline float3 extract_direction(const float4x4& m)
{
    return normalize((const float3&)m[2]);
}

float4x4 operator*(const float4x4 &a, const float4x4 &b);
float4x4 invert(const float4x4& x);



struct CameraData
{
    float4x4 view;
    float4x4 proj;
    union {
        float3 position;
        float4 position4;
    };
    float near_plane;
    float far_plane;
    float fov;
    float pad1;
};

enum class RenderFlag : uint32_t
{
    CullBackFace            = 0x0001,
    IgnoreSelfShadow        = 0x0002,
    KeepSelfDropShadow      = 0x0004,
    GPUSkinning             = 0x0100,
    ClampBlendShapeWights   = 0x0200,
};

enum class LightType : uint32_t
{
    Directional = 1,
    Spot        = 2,
    Point       = 3,
    ReversePoint= 4,
};

enum class HitMask : uint8_t
{
    Receiver    = 0x0001,
    Caster      = 0x0002,
    All = Receiver | Caster,
};

enum class UpdateFlag : uint32_t
{
    None = 0,
    Transform = 1,
    Blendshape = 2,
    Bone = 4,
};

struct LightData
{
    LightType light_type{};
    uint32_t pad[3];

    float3 position{};
    float range{};
    float3 direction{};
    float spot_angle{}; // radian
};


#define kMaxLights 32

struct SceneData
{
    CameraData camera;

    uint32_t render_flags; // combination of RenderFlag
    uint32_t light_count;
    uint32_t pad1[2];

    float shadow_ray_offset;
    float self_shadow_threshold;
    float pad2[2];

    LightData lights[kMaxLights];

    bool operator==(SceneData& v) const { return std::memcmp(this, &v, sizeof(*this)) == 0; }
    bool operator!=(SceneData& v) const { return !(*this == v); }
};

using GPUResourcePtr = void*;

using TextureData = GPUResourcePtr;
using BufferData = GPUResourcePtr;

struct BoneWeight1
{
    float weight = 0.0f;
    int index = 0;
};
struct BoneWeight4
{
    float weight[4]{};
    int index[4]{};
};
struct SkinData
{
    std::vector<float4x4>    bindposes;
    std::vector<uint8_t>     bone_counts;
    std::vector<BoneWeight1> weights;

    bool valid() const;
};

struct BlendshapeFrameData
{
    std::vector<float3> delta;
    float weight = 0.0f;
};
struct BlendshapeData
{
    std::vector<BlendshapeFrameData> frames;
};

template<class T> void addref(T *v) { v->addref(); }
template<class T> void release(T *v) { v->release(); }

template<class T>
class ref_ptr
{
public:
    ref_ptr() {}
    ref_ptr(T *data) { reset(data); }
    ref_ptr(T&& data) { swap(data); }
    ref_ptr(const ref_ptr& v) { reset(v.m_ptr); }
    ref_ptr& operator=(const ref_ptr& v) { reset(v.m_ptr); return *this; }
    ~ref_ptr() { reset(); }
    void reset(T *data = nullptr)
    {
        if (m_ptr)
            release<T>(m_ptr);
        m_ptr = data;
        if (m_ptr)
            addref<T>(m_ptr);
    }
    void swap(ref_ptr& v)
    {
        std::swap(m_ptr, v->m_data);
    }

    T& operator*() { return *m_ptr; }
    const T& operator*() const { return *m_ptr; }
    T* operator->() { return m_ptr; }
    const T* operator->() const { return m_ptr; }
    operator T*() { return m_ptr; }
    operator const T*() const { return m_ptr; }
    operator bool() const { return m_ptr; }
    bool operator==(const ref_ptr<T>& v) const { return m_ptr == v.m_ptr; }
    bool operator!=(const ref_ptr<T>& v) const { return m_ptr != v.m_ptr; }

private:
    T *m_ptr = nullptr;
};

class MeshData;
using MeshDataCallback = std::function<void(MeshData*)>;

class MeshData
{
public:
    GPUResourcePtr gpu_vertex_buffer = nullptr; // host
    GPUResourcePtr gpu_index_buffer = nullptr; // host
    void *cpu_vertex_buffer = nullptr;
    void *cpu_index_buffer = nullptr;
    int vertex_stride = 0; // if 0, treated as size_of_vertex_buffer / vertex_count
    int vertex_count = 0;
    int vertex_offset = 0; // in byte
    int index_stride = 0;
    int index_count = 0;
    int index_offset = 0; // in byte
    SkinData skin;
    std::vector<BlendshapeData> blendshapes;

    MeshData();
    ~MeshData();
    void addref();
    void release();
    bool valid() const;
    bool operator==(const MeshData& v) const;
    bool operator!=(const MeshData& v) const;
    bool operator<(const MeshData& v) const;

private:
    uint64_t id = 0;
    std::atomic_int ref_count = 1;
};
using MeshDataPtr = ref_ptr<MeshData>;

class MeshInstanceData;
using MeshInstanceDataCallback = std::function<void(MeshInstanceData*)>;

class MeshInstanceData
{
public:
    MeshDataPtr mesh;
    float4x4 transform = float4x4::identity();
    std::vector<float4x4> bones;
    std::vector<float> blendshape_weights;
    uint32_t update_flags = 0; // combination of UpdateFlag

    MeshInstanceData();
    ~MeshInstanceData();
    void addref();
    void release();
    bool valid() const;
    bool operator==(const MeshInstanceData& v) const;
    bool operator!=(const MeshInstanceData& v) const;
    bool operator<(const MeshInstanceData& v) const;

private:
    uint64_t id = 0;
    std::atomic_int ref_count = 1;
};
using MeshInstanceDataPtr = ref_ptr<MeshInstanceData>;

// one instance may be rendered multiple times with different hit mask in one frame.
// (e.g. one renderer render the object as a receiver, another one render it as a caster)
// so, hit mask must be separated from MeshInstanceData.
class GeometryData
{
public:
    MeshInstanceDataPtr instance;
    uint8_t hit_mask; // combination of HitMask

    bool valid() const;
};


template<class StdFuncT>
static inline void add_callback(std::vector<StdFuncT>& funcs, const StdFuncT& to_add)
{
    funcs.push_back(to_add);
}

template<class StdFuncT>
static inline void erase_callback(std::vector<StdFuncT>& funcs, const StdFuncT& to_erase)
{
    auto it = std::find_if(funcs.begin(), funcs.end(),
        [&to_erase](auto& a) { return a.target<void*>() == to_erase.target<void*>(); });
    if (it != funcs.end())
        funcs.erase(it);
}


} // namespace rths
