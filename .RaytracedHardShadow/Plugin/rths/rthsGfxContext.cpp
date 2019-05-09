#include "pch.h"
#include "rthsGfxContext.h"
#include "rthsResourceTranslator.h"

namespace rths {


static std::string g_gfx_error_log;
static std::once_flag g_gfx_once;
static GfxContext *g_gfx_context;

const std::string& GetErrorLog()
{
    return g_gfx_error_log;
}
void SetErrorLog(const char *format, ...)
{
    const int MaxBuf = 2048;
    char buf[MaxBuf];

    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    g_gfx_error_log = buf;
    va_end(args);
}


bool GfxContext::initializeInstance()
{
    std::call_once(g_gfx_once, []() {
        g_gfx_context = new GfxContext();
        if (!g_gfx_context->valid()) {
            delete g_gfx_context;
            g_gfx_context = nullptr;
        }
    });
    return g_gfx_context != nullptr;
}

void GfxContext::finalizeInstance()
{
    delete g_gfx_context;
    g_gfx_context = nullptr;
}


GfxContext* GfxContext::getInstance()
{
    return g_gfx_context;
}

static ID3D12ResourcePtr CreateBuffer(ID3D12Device5Ptr device, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Alignment = 0;
    desc.DepthOrArraySize = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Flags = flags;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.Height = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Width = size;

    ID3D12ResourcePtr ret;
    device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}


GfxContext::GfxContext()
{
    IDXGIFactory4Ptr dxgi_factory;
    ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));

    // Find the HW adapter
    IDXGIAdapter1Ptr adapter;
    for (uint32_t i = 0; dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip SW adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        // Create the device
        ID3D12Device5Ptr device;
        ::D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
        HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
        if (FAILED(hr) || features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
            SetErrorLog("DXR is not supported on this device");
        }
        else {
            m_device = device;
            break;
        }
    }

    if (m_device) {
        {
            D3D12_COMMAND_QUEUE_DESC desc = {};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmd_allocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmd_allocator, nullptr, IID_PPV_ARGS(&m_cmd_list));
        m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
        m_fence_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }
}

GfxContext::~GfxContext()
{
}

D3D12_CPU_DESCRIPTOR_HANDLE GfxContext::createRTV(ID3D12ResourcePtr resource, ID3D12DescriptorHeapPtr heap, uint32_t& usedHeapEntries, DXGI_FORMAT format)
{
    D3D12_RENDER_TARGET_VIEW_DESC desc = {};
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    desc.Format = format;
    desc.Texture2D.MipSlice = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = heap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += usedHeapEntries * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    usedHeapEntries++;
    m_device->CreateRenderTargetView(resource, &desc, rtvHandle);
    return rtvHandle;
}

void GfxContext::setRenderTarget(TextureData rt)
{
    uint32_t dummy = 0;
    m_rtv = createRTV(rt.resource, m_desc_heap, dummy, DXGI_FORMAT_R32_FLOAT);
}

void GfxContext::setMeshes(std::vector<MeshBuffers>& meshes)
{
    // setup geometries
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geom_descs;
    geom_descs.resize(meshes.size());
    size_t num_meshes = meshes.size();
    for (size_t i = 0; i < num_meshes; ++i) {
        auto& mesh = meshes[i];

        auto& geom_desc = geom_descs[i];
        geom_desc = {};
        geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
        if (mesh.vertex_buffer.resource) {
            geom_desc.Triangles.VertexBuffer.StartAddress = mesh.vertex_buffer.resource->GetGPUVirtualAddress();
            geom_desc.Triangles.VertexBuffer.StrideInBytes = mesh.vertex_buffer.size / mesh.vertex_count;
            geom_desc.Triangles.VertexCount = mesh.vertex_count;
            geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        }
        if (mesh.index_buffer.resource) {
            geom_desc.Triangles.IndexBuffer = mesh.index_buffer.resource->GetGPUVirtualAddress() + (sizeof(int32_t) * mesh.index_offset);
            geom_desc.Triangles.IndexCount = mesh.index_count;
            geom_desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        }
        if (mesh.transform_buffer.resource) {
            geom_desc.Triangles.Transform3x4 = mesh.transform_buffer.resource->GetGPUVirtualAddress();
        }
    }


    // Get the size requirements for the scratch and AS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
    inputs.NumDescs = (UINT)geom_descs.size();
    inputs.pGeometryDescs = geom_descs.data();
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
    static const D3D12_HEAP_PROPERTIES heap_props =
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };
    m_as_buffers.scratch = CreateBuffer(m_device, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, heap_props);
    m_as_buffers.result = CreateBuffer(m_device, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, heap_props);

    // Create the bottom-level AS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = m_as_buffers.result->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = m_as_buffers.scratch->GetGPUVirtualAddress();

    m_cmd_list->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uav_barrier = {};
    uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uav_barrier.UAV.pResource = m_as_buffers.result;
    m_cmd_list->ResourceBarrier(1, &uav_barrier);
}


bool GfxContext::valid() const
{
    return m_device != nullptr;
}

ID3D12Device5* GfxContext::getDevice()
{
    return m_device;
}

TextureData GfxContext::translateTexture(void *ptr)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->createTemporaryRenderTarget(ptr);
    return {};
}

BufferData GfxContext::translateVertexBuffer(void *ptr)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->translateVertexBuffer(ptr);
    return {};
}

BufferData GfxContext::translateIndexBuffer(void *ptr)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->translateIndexBuffer(ptr);
    return {};
}

BufferData GfxContext::allocateTransformBuffer(const float4x4& trans)
{
    BufferData ret;
    static const D3D12_HEAP_PROPERTIES heap_props =
    {
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN,
        0,
        0
    };
    ret.size = sizeof(float) * 12;
    ret.resource = CreateBuffer(m_device, ret.size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, heap_props);
    return ret;
}

void GfxContext::addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    m_cmd_list->ResourceBarrier(1, &barrier);
}

void GfxContext::flush()
{
    addResourceBarrier(m_render_target, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_cmd_list->ClearRenderTargetView(m_rtv, clear_color, 0, nullptr);

    addResourceBarrier(m_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    {
        m_cmd_list->Close();
        ID3D12CommandList* cmd_list = m_cmd_list.GetInterfacePtr();
        m_cmd_queue->ExecuteCommandLists(1, &cmd_list);
        m_fence_value++;
        m_cmd_queue->Signal(m_fence, m_fence_value);
        m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);
    }

}

void GfxContext::finish()
{
    WaitForSingleObject(m_fence_event, INFINITE);
    m_cmd_allocator->Reset();
    m_cmd_list->Reset(m_cmd_allocator, nullptr);
}

} // namespace rths


// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    using namespace rths;
    GfxContext::initializeInstance();

    auto* graphics = unityInterfaces->Get<IUnityGraphics>();
    switch (graphics->GetRenderer()) {
    case kUnityGfxRendererD3D11:
        InitializeResourceTranslator(unityInterfaces->Get<IUnityGraphicsD3D11>()->GetDevice());
        break;
    case kUnityGfxRendererD3D12:
        InitializeResourceTranslator(unityInterfaces->Get<IUnityGraphicsD3D12>()->GetDevice());
        break;
    default:
        // graphics API not supported
        SetErrorLog("Graphics API must be D3D11 or D3D12");
        break;
    }
}