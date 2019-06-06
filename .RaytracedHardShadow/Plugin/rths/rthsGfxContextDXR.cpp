#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsResourceTranslatorDXR.h"

// shader binaries
#include "rthsShaderDXR.h"


namespace rths {

static const WCHAR* kRayGenShader = L"RayGen";
static const WCHAR* kMissShader = L"Miss";
static const WCHAR* kClosestHitShader = L"Hit";
static const WCHAR* kHitGroup = L"HitGroup";

const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};
const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};
const D3D12_HEAP_PROPERTIES kReadbackHeapProps =
{
    D3D12_HEAP_TYPE_READBACK,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};


static inline std::string ToString(ID3DBlob *blob)
{
    std::string ret;
    ret.resize(blob->GetBufferSize());
    memcpy(&ret[0], blob->GetBufferPointer(), blob->GetBufferSize());
    return ret;
}



static std::once_flag g_gfx_once;
static GfxContextDXR *g_gfx_context;

bool GfxContextDXR::initializeInstance()
{
    std::call_once(g_gfx_once, []() {
        g_gfx_context = new GfxContextDXR();
        if (!g_gfx_context->valid()) {
            delete g_gfx_context;
            g_gfx_context = nullptr;
        }
    });
    return g_gfx_context != nullptr;
}

void GfxContextDXR::finalizeInstance()
{
    delete g_gfx_context;
    g_gfx_context = nullptr;
}


GfxContextDXR* GfxContextDXR::getInstance()
{
    return g_gfx_context;
}

GfxContextDXR::GfxContextDXR()
{
    initializeDevice();
}

GfxContextDXR::~GfxContextDXR()
{
}

ID3D12ResourcePtr GfxContextDXR::createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    ID3D12ResourcePtr ret;
    m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

bool GfxContextDXR::initializeDevice()
{
#ifdef rthsEnableD3D12DebugLayer
    {
        // enable d3d12 debug features
        ID3D12DebugPtr debug0;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debug0))))
        {
            // enable debug layer
            debug0->EnableDebugLayer();

#ifdef rthsEnableD3D12GBV
            ID3D12Debug1Ptr debug1;
            if (SUCCEEDED(debug0->QueryInterface(IID_PPV_ARGS(&debug1)))) {
                debug1->SetEnableGPUBasedValidation(true);
                debug1->SetEnableSynchronizedCommandQueueValidation(true);
            }
#endif
        }
    }
#endif

#ifdef rthsEnableD3D12DREAD
    {
        ID3D12DeviceRemovedExtendedDataSettingsPtr dread_settings;
        if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&dread_settings)))) {
            dread_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            dread_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
    }
#endif

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
        HRESULT hr = ::D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
        if (FAILED(hr)) {
            continue;
        }

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
        hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
        if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
            m_device = device;
            break;
        }
    }
    if (!m_device) {
        SetErrorLog("DXR is not supported on this device");
        return false;
    }

    // command queue related objects
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmd_allocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmd_allocator, nullptr, IID_PPV_ARGS(&m_cmd_list));
        m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
        m_fence_event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

#ifdef rthsDebug
    // command queue for read back (debug)
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue_copy));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_cmd_allocator_copy));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_cmd_allocator_copy, nullptr, IID_PPV_ARGS(&m_cmd_list_copy));
    }
#endif

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = 64;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvuav_heap));
        m_srvuav_cpu_handle_base = m_srvuav_heap->GetCPUDescriptorHandleForHeapStart();
        m_srvuav_gpu_handle_base = m_srvuav_heap->GetGPUDescriptorHandleForHeapStart();
        m_desc_handle_stride = m_device->GetDescriptorHandleIncrementSize(desc.Type);

        auto alloc_handle = [this]() {
            Descriptor ret;
            ret.hcpu = m_srvuav_cpu_handle_base;
            ret.hgpu = m_srvuav_gpu_handle_base;
            m_srvuav_cpu_handle_base.ptr += m_desc_handle_stride;
            m_srvuav_gpu_handle_base.ptr += m_desc_handle_stride;
            return ret;
        };

        m_render_target_handle = alloc_handle();
        m_tlas_handle = alloc_handle();
        m_scene_buffer_handle = alloc_handle();
    }

    // scene constant buffer
    {
        // size of constant buffer must be multiple of 256
        int cb_size = align_to(256, sizeof(SceneData));
        m_scene_buffer = createBuffer(cb_size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
        cbv_desc.BufferLocation = m_scene_buffer->GetGPUVirtualAddress();
        cbv_desc.SizeInBytes = cb_size;
        m_device->CreateConstantBufferView(&cbv_desc, m_scene_buffer_handle.hcpu);
    }

    // local root signature
    {
        D3D12_DESCRIPTOR_RANGE ranges[] = {
            // gOutput
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0 },
            // gRtScene
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 1 },
            // gScene
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 2 },
        };

        D3D12_ROOT_PARAMETER params[_countof(ranges)];
        for (int i = 0; i < _countof(ranges); i++) {
            auto& param = params[i];
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.DescriptorTable.NumDescriptorRanges = 1;
            param.DescriptorTable.pDescriptorRanges = &ranges[i];
            param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        }

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = _countof(params);
        desc.pParameters = params;
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

        ID3DBlobPtr sig_blob;
        ID3DBlobPtr error_blob;
        HRESULT hr = ::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
        if (FAILED(hr)) {
            SetErrorLog(ToString(error_blob) + "\n");
        }
        else {
            hr = m_device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&m_local_rootsig));
            if (FAILED(hr)) {
                SetErrorLog("CreateRootSignature() failed\n");
            }
        }
    }

    // global root signature (empty for now)
    {
        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlobPtr sig_blob;
        ID3DBlobPtr error_blob;
        HRESULT hr = ::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
        if (FAILED(hr)) {
            SetErrorLog(ToString(error_blob) + "\n");
        }
        else {
            hr = m_device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&m_global_rootsig));
            if (FAILED(hr)) {
                SetErrorLog("CreateRootSignature() failed\n");
            }
        }
    }

    // setup pipeline state
    {
        std::vector<D3D12_STATE_SUBOBJECT> subobjects;
        // keep elements' address to use D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION::pSubobjectToAssociate
        subobjects.reserve(32);

        auto add_subobject = [&subobjects](D3D12_STATE_SUBOBJECT_TYPE type, auto *ptr) {
            D3D12_STATE_SUBOBJECT so;
            so.Type = type;
            so.pDesc = ptr;
            subobjects.push_back(so);
        };

        D3D12_EXPORT_DESC export_descs[] = {
            { kRayGenShader,     nullptr, D3D12_EXPORT_FLAG_NONE },
            { kMissShader,       nullptr, D3D12_EXPORT_FLAG_NONE },
            { kClosestHitShader, nullptr, D3D12_EXPORT_FLAG_NONE },
        };
        LPCWSTR exports[] = { kRayGenShader, kMissShader, kHitGroup };

        D3D12_DXIL_LIBRARY_DESC dxil_desc{};
        dxil_desc.DXILLibrary.pShaderBytecode = rthsShaderDXR;
        dxil_desc.DXILLibrary.BytecodeLength = sizeof(rthsShaderDXR);
        dxil_desc.NumExports = _countof(export_descs);
        dxil_desc.pExports = export_descs;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxil_desc);

        D3D12_HIT_GROUP_DESC hit_desc{};
        hit_desc.HitGroupExport = kHitGroup;
        hit_desc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hit_desc.AnyHitShaderImport = nullptr;
        hit_desc.ClosestHitShaderImport = kClosestHitShader;
        hit_desc.IntersectionShaderImport = nullptr;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_desc);

        D3D12_RAYTRACING_SHADER_CONFIG rt_shader_desc{};
        rt_shader_desc.MaxPayloadSizeInBytes = sizeof(float) * 4;
        rt_shader_desc.MaxAttributeSizeInBytes = sizeof(float) * 2; // size of BuiltInTriangleIntersectionAttributes
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &rt_shader_desc);

        ID3D12RootSignature *local_rootsig = m_local_rootsig.GetInterfacePtr();
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &local_rootsig);

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION ass_desc{};
        ass_desc.pSubobjectToAssociate = &subobjects.back();
        ass_desc.NumExports = _countof(exports);
        ass_desc.pExports = exports;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &ass_desc);

        ID3D12RootSignature *global_rootsig = m_global_rootsig.GetInterfacePtr();
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &global_rootsig);

        D3D12_RAYTRACING_PIPELINE_CONFIG rt_pipeline_desc{};
        rt_pipeline_desc.MaxTraceRecursionDepth = rthsMaxBounce;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &rt_pipeline_desc);

        D3D12_STATE_OBJECT_DESC pso_desc{};
        pso_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        pso_desc.pSubobjects = subobjects.data();
        pso_desc.NumSubobjects = (UINT)subobjects.size();

        auto hr = m_device->CreateStateObject(&pso_desc, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateStateObject() failed\n");
        }
    }
    return true;
}

void GfxContextDXR::setSceneData(SceneData& data)
{
    SceneData *dst;
    auto hr = m_scene_buffer->Map(0, nullptr, (void**)&dst);
    if (FAILED(hr)) {
        SetErrorLog("m_scene_buffer->Map() failed\n");
    }
    else {
        *dst = data;
        m_scene_buffer->Unmap(0, nullptr);
    }
}

void GfxContextDXR::setRenderTarget(TextureData& rt)
{
    auto& record = m_texture_records[identifier(rt)];
    if (!record.data.resource) {
        record.data = GetResourceTranslator(m_device)->createTemporaryTexture(rt.texture);
        if (!record.data.resource) {
            DebugPrint("GfxContextDXR::setRenderTarget(): failed to translate texture\n");
            return;
        }
    }
    ++record.used;

    m_render_target = record.data;

    auto desc = m_render_target.resource->GetDesc();
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Format = desc.Format;
    uav_desc.Texture2D.MipSlice = 0;
    uav_desc.Texture2D.PlaneSlice = 0;
    m_device->CreateUnorderedAccessView(m_render_target.resource, nullptr, &uav_desc, m_render_target_handle.hcpu);

#ifdef rthsEnableRenderTargetValidation
    // fill texture with 0.0-1.0 gradation for debug
    {
        int n = m_render_target.width * m_render_target.height;
        float r = 1.0f / (float)n;

        std::vector<float> data;
        data.resize(n);
        for (int i = 0; i < n; ++i)
            data[i] = r * (float)i;
        uploadTexture(m_render_target.resource, data.data(), m_render_target.width, m_render_target.height, sizeof(float));
    }
#endif
}

void GfxContextDXR::setMeshes(std::vector<MeshData>& meshes)
{
    m_mesh_instances.clear();

    auto get_vertex_buffer = [this](void *buffer) {
        auto& record = m_buffer_records[buffer];
        ++record.used;
        if (!record.data.resource)
            record.data = GetResourceTranslator(m_device)->translateVertexBuffer(buffer);
        return record.data;
    };

    auto get_index_buffer = [this](void *buffer) {
        auto& record = m_buffer_records[buffer];
        ++record.used;
        if (!record.data.resource)
            record.data = GetResourceTranslator(m_device)->translateIndexBuffer(buffer);
        return record.data;
    };

    size_t num_meshes = meshes.size();
    for (size_t i = 0; i < num_meshes; ++i) {
        auto& src = meshes[i];
        auto& record = m_mesh_records[identifier(src)];
        ++record.used;
        auto& data = record.data;

        if (!data) {
            data.reset(new MeshDataDXR());
            data->vertex_count = src.vertex_count;
            data->index_bits = src.index_bits;
            data->index_count = src.index_count;
            data->index_offset = src.index_offset;
        }
        if (!data->vertex_buffer.resource) {
            data->vertex_buffer = get_vertex_buffer(src.vertex_buffer);
            if (!data->vertex_buffer.resource) {
                DebugPrint("GfxContextDXR::setMeshes(): failed to translate vertex buffer\n");
                continue;
            }
            if ((data->vertex_buffer.size / data->vertex_count) % 4 != 0) {
                DebugPrint("GfxContextDXR::setMeshes(): unrecognizable vertex format\n");
                continue;
            }

#ifdef rthsEnableBufferValidation
            {
                // inspect buffer
                std::vector<float> vertex_buffer_data;
                vertex_buffer_data.resize(data->vertex_buffer.size / sizeof(float), std::numeric_limits<float>::quiet_NaN());
                readbackBuffer(vertex_buffer_data.data(), data->vertex_buffer.resource, data->vertex_buffer.size);
            }
#endif
        }
        if (!data->index_buffer.resource) {
            data->index_buffer = get_index_buffer(src.index_buffer);
            if (!data->index_buffer.resource) {
                DebugPrint("GfxContextDXR::setMeshes(): failed to translate index buffer\n");
                continue;
            }

#ifdef rthsEnableBufferValidation
            {
                // inspect buffer
                std::vector<uint32_t> index_buffer_data32;
                std::vector<uint16_t> index_buffer_data16;
                if (data->index_bits == 16) {
                    index_buffer_data16.resize(data->index_buffer.size / sizeof(uint16_t), std::numeric_limits<uint16_t>::max());
                    readbackBuffer(index_buffer_data16.data(), data->index_buffer.resource, data->index_buffer.size);
                }
                else {
                    index_buffer_data32.resize(data->index_buffer.size / sizeof(uint32_t), std::numeric_limits<uint32_t>::max());
                    readbackBuffer(index_buffer_data32.data(), data->index_buffer.resource, data->index_buffer.size);
                }
            }
#endif
        }

        // build bottom level acceleration structure
        if (!data->blas) {
            D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
            geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
            if (data->vertex_buffer.resource) {
                geom_desc.Triangles.VertexBuffer.StartAddress = data->vertex_buffer.resource->GetGPUVirtualAddress();
                geom_desc.Triangles.VertexBuffer.StrideInBytes = data->vertex_buffer.size / data->vertex_count;
                geom_desc.Triangles.VertexCount = data->vertex_count;
                geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            }
            if (data->index_buffer.resource) {
                int index_size = data->index_bits / 8;
                geom_desc.Triangles.IndexBuffer = data->index_buffer.resource->GetGPUVirtualAddress() + (index_size * data->index_offset);
                geom_desc.Triangles.IndexCount = data->index_count;
                geom_desc.Triangles.IndexFormat = data->index_bits == 16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
            }
            // note: transform is handled by top level acceleration structure

            // Get the size requirements for the scratch and AS buffers
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
            inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
            inputs.NumDescs = 1;
            inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            inputs.pGeometryDescs = &geom_desc;

            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
            m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
            auto scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
            m_temporary_buffers.push_back(scratch);
            data->blas = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

            // Create the bottom-level AS
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
            as_desc.Inputs = inputs;
            as_desc.DestAccelerationStructureData = data->blas->GetGPUVirtualAddress();
            as_desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();

            m_cmd_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
        }

        m_mesh_instances.push_back({data, src.transform});
    }

    num_meshes = m_mesh_instances.size();

    // build top level acceleration structures
    {
        // First, get the size of the TLAS buffers and create them
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = (UINT)num_meshes;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        // Create the buffers
        auto scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
        m_temporary_buffers.push_back(scratch);
        m_tlas = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
        uint64_t tlas_size = info.ResultDataMaxSizeInBytes;

        // The instance desc should be inside a buffer, create and map the buffer
        auto instance_descs_buf = createBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_meshes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        m_temporary_buffers.push_back(instance_descs_buf);
        D3D12_RAYTRACING_INSTANCE_DESC* instance_descs;
        instance_descs_buf->Map(0, nullptr, (void**)&instance_descs);
        ZeroMemory(instance_descs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_meshes);
        for (uint32_t i = 0; i < num_meshes; i++) {
            auto& instance = m_mesh_instances[i];

            (float3x4&)instance_descs[i].Transform = instance.transform;
            instance_descs[i].InstanceID = i; // This value will be exposed to the shader via InstanceID()
            instance_descs[i].InstanceMask = 0xFF;
            instance_descs[i].InstanceContributionToHitGroupIndex = i;
            instance_descs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE; // D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE
            instance_descs[i].AccelerationStructure = instance.mesh->blas->GetGPUVirtualAddress();
        }
        instance_descs_buf->Unmap(0, nullptr);

        // Create the TLAS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
        as_desc.DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress();
        as_desc.Inputs = inputs;
        as_desc.Inputs.InstanceDescs = instance_descs_buf->GetGPUVirtualAddress();
        as_desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();

        m_cmd_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        D3D12_RESOURCE_BARRIER uav_barrier{};
        uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uav_barrier.UAV.pResource = m_tlas;
        m_cmd_list->ResourceBarrier(1, &uav_barrier);

        // Create the TLAS SRV
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.RaytracingAccelerationStructure.Location = m_tlas->GetGPUVirtualAddress();
        m_device->CreateShaderResourceView(nullptr, &srv_desc, m_tlas_handle.hcpu);
    }
}


bool GfxContextDXR::valid() const
{
    return m_device != nullptr;
}

bool GfxContextDXR::validateDevice()
{
    if (!m_device)
        return false;

    auto reason = m_device->GetDeviceRemovedReason();
    if (reason != 0) {
#ifdef rthsEnableD3D12DREAD
        {
            ID3D12DeviceRemovedExtendedDataPtr dread;
            if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&dread)))) {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT DredAutoBreadcrumbsOutput;
                D3D12_DRED_PAGE_FAULT_OUTPUT DredPageFaultOutput;
                dread->GetAutoBreadcrumbsOutput(&DredAutoBreadcrumbsOutput);
                dread->GetPageFaultAllocationOutput(&DredPageFaultOutput);
                // todo: get error log
            }
        }
#endif

        PSTR buf = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, reason, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

        std::string message(buf, size);
        SetErrorLog(message.c_str());
        return false;
    }
    return true;
}

ID3D12Device5* GfxContextDXR::getDevice()
{
    return m_device;
}

void GfxContextDXR::addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    m_cmd_list->ResourceBarrier(1, &barrier);
}

uint64_t GfxContextDXR::submitCommandList()
{
    m_cmd_list->Close();
    ID3D12CommandList* cmd_list = m_cmd_list.GetInterfacePtr();
    m_cmd_queue->ExecuteCommandLists(1, &cmd_list);
    m_fence_value++;
    m_cmd_queue->Signal(m_fence, m_fence_value);
    m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);
    return m_fence_value;
}


bool GfxContextDXR::readbackBuffer(void *dst, ID3D12Resource *src, size_t size)
{
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    m_cmd_list_copy->CopyBufferRegion(readback_buf, 0, src, 0, size);
    m_cmd_list_copy->Close();

    ID3D12CommandList* cmd_list_copy = m_cmd_list_copy.GetInterfacePtr();
    m_cmd_queue_copy->ExecuteCommandLists(1, &cmd_list_copy);
    m_fence_value++;
    m_cmd_queue_copy->Signal(m_fence, m_fence_value);
    m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);

    ::WaitForSingleObject(m_fence_event, INFINITE);
    m_cmd_allocator_copy->Reset();
    m_cmd_list_copy->Reset(m_cmd_allocator_copy, nullptr);

    float* mapped;
    if (SUCCEEDED(readback_buf->Map(0, nullptr, (void**)&mapped))) {
        memcpy(dst, mapped, size);
        // * break here to check data *
        readback_buf->Unmap(0, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::uploadBuffer(ID3D12Resource *dst, const void *src, size_t size)
{
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    void *mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, &mapped))) {
        memcpy(mapped, src, size);
        upload_buf->Unmap(0, nullptr);

        m_cmd_list_copy->CopyBufferRegion(dst, 0, upload_buf, 0, size);
        m_cmd_list_copy->Close();

        ID3D12CommandList* cmd_list_copy = m_cmd_list_copy.GetInterfacePtr();
        m_cmd_queue_copy->ExecuteCommandLists(1, &cmd_list_copy);
        m_fence_value++;
        m_cmd_queue_copy->Signal(m_fence, m_fence_value);
        m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);

        ::WaitForSingleObject(m_fence_event, INFINITE);
        m_cmd_allocator_copy->Reset();
        m_cmd_list_copy->Reset(m_cmd_allocator_copy, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::readbackTexture(void *dst, ID3D12Resource *src, size_t width, size_t height, size_t stride)
{
    size_t size = width * height * stride;
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    if (!readback_buf)
        return false;

    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = readback_buf;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst_loc.PlacedFootprint.Offset = 0;
    dst_loc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
    dst_loc.PlacedFootprint.Footprint.Width = (UINT)width;
    dst_loc.PlacedFootprint.Footprint.Height = (UINT)height;
    dst_loc.PlacedFootprint.Footprint.Depth = 1;
    dst_loc.PlacedFootprint.Footprint.RowPitch = (UINT)(width * stride);

    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = m_render_target.resource;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_loc.SubresourceIndex = 0;

    m_cmd_list_copy->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    m_cmd_list_copy->Close();

    ID3D12CommandList* cmd_list_copy = m_cmd_list_copy.GetInterfacePtr();
    m_cmd_queue_copy->ExecuteCommandLists(1, &cmd_list_copy);
    m_fence_value++;
    m_cmd_queue_copy->Signal(m_fence, m_fence_value);
    m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);

    ::WaitForSingleObject(m_fence_event, INFINITE);
    m_cmd_allocator_copy->Reset();
    m_cmd_list_copy->Reset(m_cmd_allocator_copy, nullptr);

    float* mapped;
    if (SUCCEEDED(readback_buf->Map(0, nullptr, (void**)&mapped))) {
        memcpy(dst, mapped, size);
        readback_buf->Unmap(0, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::uploadTexture(ID3D12Resource *dst, const void *src, size_t width, size_t height, size_t stride)
{
    size_t size = width * height * stride;
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    if (!upload_buf)
        return false;

    void *mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, &mapped))) {
        memcpy(mapped, src, size);
        upload_buf->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION dst_loc{};
        dst_loc.pResource = m_render_target.resource;
        dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_loc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = upload_buf;
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_loc.PlacedFootprint.Offset = 0;
        src_loc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_FLOAT;
        src_loc.PlacedFootprint.Footprint.Width = (UINT)width;
        src_loc.PlacedFootprint.Footprint.Height = (UINT)height;
        src_loc.PlacedFootprint.Footprint.Depth = 1;
        src_loc.PlacedFootprint.Footprint.RowPitch = (UINT)(width * stride);

        m_cmd_list_copy->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
        m_cmd_list_copy->Close();

        ID3D12CommandList* cmd_list_copy = m_cmd_list_copy.GetInterfacePtr();
        m_cmd_queue_copy->ExecuteCommandLists(1, &cmd_list_copy);
        m_fence_value++;
        m_cmd_queue_copy->Signal(m_fence, m_fence_value);
        m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);

        ::WaitForSingleObject(m_fence_event, INFINITE);
        m_cmd_allocator_copy->Reset();
        m_cmd_list_copy->Reset(m_cmd_allocator_copy, nullptr);
        return true;
    }
    return false;
}

void GfxContextDXR::sync()
{
    submitCommandList();
    ::WaitForSingleObject(m_fence_event, INFINITE);

    m_cmd_allocator->Reset();
    m_cmd_list->Reset(m_cmd_allocator, nullptr);
}

void GfxContextDXR::flush()
{
    if (!m_render_target.resource) {
        SetErrorLog("GfxContext::flush(): render target is null\n");
        return;
    }
    if (m_flushing) {
        SetErrorLog("GfxContext::flush(): called before finish()\n");
        return;
    }

    int num_meshes = (int)m_mesh_instances.size();

    // setup shader table
    {
        // ray-gen + miss + hit for each meshes
        int required_count = 2 + num_meshes;

        m_shader_record_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        m_shader_record_size += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);
        m_shader_record_size = align_to(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, m_shader_record_size);

        // allocate new buffer if required count exceeds capacity
        if (required_count > m_shader_table_entry_capacity) {
            int new_capacity = std::max<int>(required_count, std::max<int>(m_shader_table_entry_capacity * 2, 1024));
            m_shader_table = createBuffer(m_shader_record_size * new_capacity, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
            m_shader_table_entry_capacity = new_capacity;
        }

        // setup shader table entries
        if (required_count > m_shader_table_entry_count) {
            uint8_t *data;
            m_shader_table->Map(0, nullptr, (void**)&data);

            ID3D12StateObjectPropertiesPtr sop;
            m_pipeline_state->QueryInterface(IID_PPV_ARGS(&sop));

            auto add_shader_table = [&data, this](void *shader_id) {
                auto dst = data;
                memcpy(dst, shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                dst += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
                *(UINT64*)(dst) = m_srvuav_heap->GetGPUDescriptorHandleForHeapStart().ptr;

                data += m_shader_record_size;
            };

            // ray-gen
            add_shader_table(sop->GetShaderIdentifier(kRayGenShader));

            // miss
            add_shader_table(sop->GetShaderIdentifier(kMissShader));

            // hit for each meshes
            void *hit = sop->GetShaderIdentifier(kHitGroup);
            for (int i = 0; i < num_meshes; ++i)
                add_shader_table(hit);

            m_shader_table->Unmap(0, nullptr);
            m_shader_table_entry_count = required_count;
        }
    }

    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // dispatch rays
    {
        D3D12_DISPATCH_RAYS_DESC dr_desc{};
        dr_desc.Width = m_render_target.width;
        dr_desc.Height = m_render_target.height;
        dr_desc.Depth = 1;

        auto addr = m_shader_table->GetGPUVirtualAddress();
        // ray-gen
        dr_desc.RayGenerationShaderRecord.StartAddress = addr;
        dr_desc.RayGenerationShaderRecord.SizeInBytes = m_shader_record_size;
        addr += m_shader_record_size;

        // miss
        dr_desc.MissShaderTable.StartAddress = addr;
        dr_desc.MissShaderTable.StrideInBytes = m_shader_record_size;
        dr_desc.MissShaderTable.SizeInBytes = m_shader_record_size; 
        addr += m_shader_record_size;

        // hit for each meshes
        dr_desc.HitGroupTable.StartAddress = addr;
        dr_desc.HitGroupTable.StrideInBytes = m_shader_record_size;
        dr_desc.HitGroupTable.SizeInBytes = m_shader_record_size * num_meshes;

        // descriptor heaps
        ID3D12DescriptorHeap *desc_heaps[] = {
            m_srvuav_heap,
            // sampler heap will be here
        };
        m_cmd_list->SetDescriptorHeaps(_countof(desc_heaps), desc_heaps);

        // bind root signature and shader resources
        m_cmd_list->SetComputeRootSignature(m_global_rootsig);

        // dispatch
        m_cmd_list->SetPipelineState1(m_pipeline_state.GetInterfacePtr());
        m_cmd_list->DispatchRays(&dr_desc);
    }

    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

    submitCommandList();
    m_flushing = true;
}

void GfxContextDXR::finish()
{
    if (m_flushing) {
        ::WaitForSingleObject(m_fence_event, INFINITE);
        m_flushing = false;
    }

    m_cmd_allocator->Reset();
    m_cmd_list->Reset(m_cmd_allocator, nullptr);

#ifdef rthsEnableRenderTargetValidation
    {
        std::vector<float> data;
        data.resize(m_render_target.width * m_render_target.height, std::numeric_limits<float>::quiet_NaN());
        readbackTexture(data.data(), m_render_target.resource, m_render_target.width, m_render_target.height, sizeof(float));
        // break here to inspect data
    }
#endif

    // copy render target content to Unity side
    GetResourceTranslator(m_device)->applyTexture(m_render_target);
}

void GfxContextDXR::releaseUnusedResources()
{
    // erase unused texture / buffer / mesh data
    auto erase_unused_records = [](auto& records, const char *message) {
        int num_erased = 0;
        for (auto it = records.begin(); it != records.end(); /**/) {
            if (it->second.used == 0) {
                records.erase(it++);
                ++num_erased;
            }
            else {
                it->second.used = 0;
                ++it;
            }
        }
        if (num_erased > 0) {
            DebugPrint(message, num_erased);
        }
    };
    erase_unused_records(m_texture_records, "GfxContextDXR::releaseUnusedResources(): erased %d texture records\n");
    erase_unused_records(m_buffer_records, "GfxContextDXR::releaseUnusedResources(): erased %d buffer records\n");
    erase_unused_records(m_mesh_records, "GfxContextDXR::releaseUnusedResources(): erased %d mesh records\n");

    m_temporary_buffers.clear();
}

} // namespace rths
#endif
