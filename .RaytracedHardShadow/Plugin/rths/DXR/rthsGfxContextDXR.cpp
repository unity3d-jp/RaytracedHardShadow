#include "pch.h"
#ifdef _WIN32
#include "Foundation/rthsLog.h"
#include "Foundation/rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsResourceTranslatorDXR.h"

// shader binaries
#include "rthsShadowDXR.hlsl.h"


namespace rths {

struct InstanceData
{
    uint32_t related_caster_mask;
};

extern ID3D12Device *g_host_d3d12_device;

static const int kMaxTraceRecursionLevel = 2;

static const WCHAR* kRayGenShader1 = L"RayGen";
static const WCHAR* kRayGenShader2 = L"RayGenWithAdaptiveSampling";
static const WCHAR* kMissShader1 = L"Miss1";
static const WCHAR* kMissShader2 = L"Miss2";
static const WCHAR* kAnyHitShader = L"AnyHit";
static const WCHAR* kClosestHitShader = L"ClosestHit";
static const WCHAR* kHitGroup1 = L"HitGroup1";
static const WCHAR* kHitGroup2 = L"HitGroup2";

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



static int g_gfx_initialize_count = 0;
static std::unique_ptr<GfxContextDXR> g_gfx_context;

bool GfxContextDXR::initializeInstance()
{
    if (g_gfx_initialize_count++ == 0) {
        g_gfx_context = std::make_unique<GfxContextDXR>();
        if (!g_gfx_context->valid())
            g_gfx_context.reset();
    }
    return g_gfx_context != nullptr;
}

void GfxContextDXR::finalizeInstance()
{
    if (--g_gfx_initialize_count == 0) {
        g_gfx_context.reset();
    }
}


GfxContextDXR* GfxContextDXR::getInstance()
{
    return g_gfx_context.get();
}

GfxContextDXR::GfxContextDXR()
{
    if (!initialize())
        return;
}

GfxContextDXR::~GfxContextDXR()
{
}

bool GfxContextDXR::initialize()
{
    // check host d3d12 device
    if (g_host_d3d12_device) {
        auto hr = g_host_d3d12_device->QueryInterface(&m_device);
        if (SUCCEEDED(hr)) {
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
            hr = g_host_d3d12_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
            if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
                m_device = g_host_d3d12_device;
            }
        }
    }

    // try to create d3d12 device
    if (!m_device) {
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
#endif // rthsEnableD3D12GBV
            }
        }
#endif // rthsEnableD3D12DebugLayer

#ifdef rthsEnableD3D12DREAD
        {
            ID3D12DeviceRemovedExtendedDataSettingsPtr dread_settings;
            if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&dread_settings)))) {
                dread_settings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
                dread_settings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            }
        }
#endif // rthsEnableD3D12DREAD

        IDXGIFactory4Ptr dxgi_factory;
        ::CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));

        // find DXR capable adapter and create device
        auto create_device = [this, dxgi_factory](bool software) -> ID3D12Device5Ptr {
            IDXGIAdapter1Ptr adapter;
            for (uint32_t i = 0; dxgi_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (!software && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                    continue;
                else if (software && (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
                    continue;

                // Create the device
                ID3D12Device5Ptr device;
                HRESULT hr = ::D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
                if (SUCCEEDED(hr)) {
                    D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5{};
                    hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
                    if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
                        return device;
                    }
                }
            }
            return nullptr;
        };

#ifdef rthsForceSoftwareDevice
        m_device = create_device(true);
#else // rthsForceSoftwareDevice
        m_device = create_device(false);
        if(!m_device)
            m_device = create_device(true);
#endif // rthsForceSoftwareDevice
    }

    // failed to create device (DXR is not supported)
    if (!m_device) {
        SetErrorLog("DXR is not supported on this device");
        return false;
    }

    // resource translator (null if there is no host device)
    m_resource_translator = CreateResourceTranslator();

    // fence
    // creating a sharable fence on d3d12 and share it with d3d11 seems don't work (at least on my machine).
    // but creating on d3d11 and share with d3d12 works fine. so creating a fence let on the host-device side.
    if (m_resource_translator)
        m_fence = m_resource_translator->getFence(m_device);
    else
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));


    // deformer
    m_deformer = std::make_shared<DeformerDXR>(m_device);

    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue_direct));
    }
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue_compute));
    }
    {
        D3D12_COMMAND_QUEUE_DESC desc{};
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue_immediate_copy));
    }

    // root signature
    {
        D3D12_DESCRIPTOR_RANGE ranges1[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };
        D3D12_DESCRIPTOR_RANGE ranges2[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };
        D3D12_DESCRIPTOR_RANGE ranges3[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND },
        };

        D3D12_ROOT_PARAMETER params[3]{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[0].DescriptorTable.NumDescriptorRanges = _countof(ranges1);
        params[0].DescriptorTable.pDescriptorRanges = ranges1;

        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable.NumDescriptorRanges = _countof(ranges2);
        params[1].DescriptorTable.pDescriptorRanges = ranges2;

        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].DescriptorTable.NumDescriptorRanges = _countof(ranges3);
        params[2].DescriptorTable.pDescriptorRanges = ranges3;

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = _countof(params);
        desc.pParameters = params;

        ID3DBlobPtr sig_blob, error_blob;
        HRESULT hr = ::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
        if (FAILED(hr)) {
            SetErrorLog(ToString(error_blob) + "\n");
        }
        else {
            hr = m_device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&m_rootsig));
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

        D3D12_DXIL_LIBRARY_DESC dxil_desc{};
        dxil_desc.DXILLibrary.pShaderBytecode = g_rthsShadowDXR;
        dxil_desc.DXILLibrary.BytecodeLength = sizeof(g_rthsShadowDXR);
        // zero exports means 'export all'
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxil_desc);

        D3D12_HIT_GROUP_DESC hit_desc1{};
        hit_desc1.HitGroupExport = kHitGroup1;
        hit_desc1.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hit_desc1.ClosestHitShaderImport = kClosestHitShader;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_desc1);

        D3D12_HIT_GROUP_DESC hit_desc2{};
        hit_desc2.HitGroupExport = kHitGroup2;
        hit_desc2.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hit_desc2.AnyHitShaderImport = kAnyHitShader;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &hit_desc2);

        D3D12_RAYTRACING_SHADER_CONFIG rt_shader_desc{};
        rt_shader_desc.MaxPayloadSizeInBytes = sizeof(float) * 4;
        rt_shader_desc.MaxAttributeSizeInBytes = sizeof(float) * 2; // size of BuiltInTriangleIntersectionAttributes
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &rt_shader_desc);

        D3D12_GLOBAL_ROOT_SIGNATURE global_rootsig{};
        global_rootsig.pGlobalRootSignature = m_rootsig;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &global_rootsig);

        D3D12_RAYTRACING_PIPELINE_CONFIG pipeline_desc{};
        pipeline_desc.MaxTraceRecursionDepth = kMaxTraceRecursionLevel;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &pipeline_desc);

        D3D12_STATE_OBJECT_DESC pso_desc{};
        pso_desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        pso_desc.pSubobjects = subobjects.data();
        pso_desc.NumSubobjects = (UINT)subobjects.size();

#ifdef rthsDebug
        PrintStateObjectDesc(&pso_desc);
#endif // rthsDebug

        auto hr = m_device->CreateStateObject(&pso_desc, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateStateObject() failed\n");
        }
    }

    return true;
}

void GfxContextDXR::clear()
{
    clearResourceCache();

    m_cmd_queue_direct = nullptr;
    m_cmd_queue_compute = nullptr;
    m_cmd_queue_immediate_copy = nullptr;

    m_pipeline_state = nullptr;
    m_rootsig = nullptr;
    m_fence = nullptr;
    m_fence_value = m_fv_last_rays = 0;

    m_resource_translator = nullptr;
    m_deformer = nullptr;
    m_device = nullptr;
}

void GfxContextDXR::frameBegin()
{
    for (auto& kvp : m_meshinstance_records) {
        kvp.second->is_updated = false;
    }
    m_fv_last_rays = 0;
}

void GfxContextDXR::prepare(RenderDataDXR& rd)
{
    if (!valid())
        return;

    // initialize command lists
    if (!rd.clm_blas) {
        rd.clm_blas = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"BLAS");
        rd.clm_tlas = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"TLAS");
        rd.clm_rays = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT, L"Rays");
        rd.clm_immediate_copy = std::make_shared<CommandListManagerDXR>(m_device, D3D12_COMMAND_LIST_TYPE_COPY, L"Copy");
    }

    // initialize desc heap
    if (!rd.desc_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = 32;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rd.desc_heap));

        auto handle_allocator = DescriptorHeapAllocatorDXR(m_device, rd.desc_heap);
        for (int i = 0; i < _countof(rd.render_target_handles); ++i)
            rd.render_target_handles[i] = handle_allocator.allocate();
        rd.tlas_handle = handle_allocator.allocate();
        rd.instance_data_handle = handle_allocator.allocate();
        rd.scene_data_handle = handle_allocator.allocate();
        for (int i = 0; i < _countof(rd.prev_result_handles); ++i)
            rd.prev_result_handles[i] = handle_allocator.allocate();
    }

    // initialize scene constant buffer
    if (!rd.scene_data) {
        // size of constant buffer must be multiple of 256
        int cb_size = align_to(256, sizeof(SceneData));
        rd.scene_data = createBuffer(cb_size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        SceneData *dst;
        if (SUCCEEDED(rd.scene_data->Map(0, nullptr, (void**)&dst))) {
            *dst = SceneData{};
            rd.scene_data->Unmap(0, nullptr);
        }

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
        cbv_desc.BufferLocation = rd.scene_data->GetGPUVirtualAddress();
        cbv_desc.SizeInBytes = cb_size;
        m_device->CreateConstantBufferView(&cbv_desc, rd.scene_data_handle.hcpu);
    }

    rthsTimestampInitialize(rd.timestamp, m_device);
    rthsTimestampReset(rd.timestamp);
    rthsTimestampSetEnable(rd.timestamp, rd.hasFlag(RenderFlag::DbgTimestamp));

    // reset fence values
    rd.fv_deform = rd.fv_blas = rd.fv_tlas = rd.fv_rays = 0;

    if (m_fv_last_rays != 0) {
        // wait for complete previous renderer's DispatchRays()
        // because there may be dependencies for the previous renderer. (e.g. building BLAS)
        m_cmd_queue_direct->Wait(m_fence, m_fv_last_rays);
    }
}

void GfxContextDXR::setSceneData(RenderDataDXR& rd, SceneData& data)
{
    if (!valid())
        return;
    if (!rd.scene_data || rd.scene_data_prev == data)
        return;

    SceneData *dst;
    if (SUCCEEDED(rd.scene_data->Map(0, nullptr, (void**)&dst))) {
        *dst = data;
        rd.scene_data->Unmap(0, nullptr);
    }
    else {
        SetErrorLog("rd.scene_data->Map() failed\n");
    }
    rd.render_flags = data.render_flags;
    rd.scene_data_prev = data;
}

void GfxContextDXR::setRenderTarget(RenderDataDXR& rd, RenderTargetData *rt)
{
    if (!valid())
        return;
    if (!rt) {
        rd.render_target = nullptr;
        return;
    }

    auto& data = m_rendertarget_records[rt];
    if (!data) {
        data = std::make_shared<RenderTargetDataDXR>();
        data->base = rt;
        if (rt->gpu_texture && m_resource_translator) {
            data->texture = m_resource_translator->createTemporaryTexture(rt->gpu_texture);
            if (!data->texture) {
                DebugPrint("GfxContextDXR::setRenderTarget(): failed to translate texture\n");
                return;
            }
        }
        else {
            auto dxgifmt = GetDXGIFormat(rt->format);
            if (rt->width > 0 && rt->height > 0 && dxgifmt != DXGI_FORMAT_UNKNOWN) {
                auto tex = std::make_shared<TextureDataDXR>();
                tex->resource = createTexture(rt->width, rt->height, dxgifmt);
                if (!tex->resource) {
                    DebugPrint("GfxContextDXR::setRenderTarget(): failed to create texture\n");
                    return;
                }
                tex->width = rt->width;
                tex->height = rt->height;
                tex->format = dxgifmt;
                data->texture = tex;
            }
        }

        int w = data->texture->width;
        int h = data->texture->height;
        if (w > 32 && h > 32) {
            data->half_res = createTexture(w / 2, h / 2, DXGI_FORMAT_R16G16_FLOAT);
            data->quarter_res = createTexture(w / 4, h / 4, DXGI_FORMAT_R16G16_FLOAT);
        }
    }

    if (rd.render_target != data) {
        rd.render_target = data;
        if (data) {
            auto create_uav = [this](auto& res, auto hcpu) {
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                uav_desc.Format = GetTypedFormatDXR(res->GetDesc().Format); // typeless is not allowed for unordered access view
                m_device->CreateUnorderedAccessView(res, nullptr, &uav_desc, hcpu);
            };

            auto create_srv = [this](auto& res, auto hcpu) {
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.Texture2D.MipLevels = 1;
                m_device->CreateShaderResourceView(res, &srv_desc, hcpu);
            };

            create_uav(data->texture->resource, rd.render_target_handles[0].hcpu);
            if (data->half_res && data->quarter_res) {
                create_uav(data->half_res, rd.render_target_handles[1].hcpu);
                create_uav(data->quarter_res, rd.render_target_handles[2].hcpu);

                create_srv(data->half_res, rd.prev_result_handles[0].hcpu);
                create_srv(data->quarter_res, rd.prev_result_handles[1].hcpu);
            }
        }
    }

#ifdef rthsEnableRenderTargetValidation
    if (rd.render_target) {
        // fill texture with 0.0-1.0 gradation for debug
        auto do_fill = [this, &rd](auto& tex, auto&& data) {
            int n = tex.width * tex.height;
            float r = 1.0f / (float)(n - 1);
            data.resize(n);
            for (int i = 0; i < n; ++i)
                data[i] = r * (float)i;
            uploadTexture(rd, tex.resource, data.data(), tex.width, tex.height, tex.format);
        };

        auto& tex = *rd.render_target->texture;
        switch (GetTypedFormatDXR(tex.format)) {
        case DXGI_FORMAT_R8_UNORM: do_fill(tex, std::vector<unorm8>()); break;
        case DXGI_FORMAT_R16_FLOAT: do_fill(tex, std::vector<half>()); break;
        case DXGI_FORMAT_R32_FLOAT: do_fill(tex, std::vector<float>()); break;
        }
    }
#endif // rthsEnableRenderTargetValidation
}

void GfxContextDXR::setGeometries(RenderDataDXR& rd, std::vector<GeometryData>& geoms)
{
    if (!valid())
        return;
    if (rd.fv_blas != 0 || rd.fv_tlas != 0) {
        SetErrorLog("GfxContext::setGeometries(): called before prepare()\n");
        return;
    }

    auto translate_gpu_buffer = [this](GPUResourcePtr buffer) {
        auto& data = m_buffer_records[buffer];
        if (!data)
            data = m_resource_translator->translateBuffer(buffer);
        return data;
    };

    auto upload_cpu_buffer = [this](const void *buffer, int size) {
        auto& data = m_buffer_records[buffer];
        if (!data)
        {
            data = std::make_shared<BufferDataDXR>();
            data->resource = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
            data->size = size;

            void *dst;
            data->resource->Map(0, nullptr, &dst);
            std::memcpy(dst, buffer, size);
            data->resource->Unmap(0, nullptr);
        }
        return data;
    };

    if (rd.hasFlag(RenderFlag::DbgForceUpdateAS)) {
        // mark updated to force update BLAS
        for (auto& geom : geoms)
            geom.markUpdated();
    }

    int task_granularity = ceildiv((int)geoms.size(), rd.max_parallel_command_lists);
    bool gpu_skinning = rd.hasFlag(RenderFlag::GPUSkinning);
    if (gpu_skinning)
        m_deformer->prepare(rd);
    int deform_count = 0;
    rd.geometries.clear();

    bool needs_build_tlas = false;
    for (auto& geom : geoms) {
        auto& inst = geom.instance;
        auto& mesh = inst->mesh;
        if (mesh->vertex_count == 0 || mesh->index_count == 0)
            continue;

        auto& mesh_dxr = m_mesh_records[mesh];
        if (!mesh_dxr) {
            mesh_dxr = std::make_shared<MeshDataDXR>();
            mesh_dxr->base = mesh;
        }

        if (!mesh_dxr->vertex_buffer) {
            if (mesh->gpu_vertex_buffer && m_resource_translator)
                mesh_dxr->vertex_buffer = translate_gpu_buffer(mesh->gpu_vertex_buffer);
            else if (mesh->cpu_vertex_buffer)
                mesh_dxr->vertex_buffer = upload_cpu_buffer(mesh->cpu_vertex_buffer, mesh->vertex_count * mesh->vertex_stride);

            if (!mesh_dxr->vertex_buffer->resource) {
                DebugPrint("GfxContextDXR::setMeshes(): failed to translate vertex buffer\n");
                continue;
            }
            if ((mesh_dxr->vertex_buffer->size / mesh->vertex_count) % 4 != 0) {
                DebugPrint("GfxContextDXR::setMeshes(): unrecognizable vertex format\n");
                continue;
            }

#ifdef rthsEnableBufferValidation
            if (mesh_dxr->vertex_buffer) {
                // inspect buffer
                std::vector<float> vertex_buffer_data;
                vertex_buffer_data.resize(mesh_dxr->vertex_buffer->size / sizeof(float), std::numeric_limits<float>::quiet_NaN());
                readbackBuffer(rd, vertex_buffer_data.data(), mesh_dxr->vertex_buffer->resource, mesh_dxr->vertex_buffer->size);
            }
#endif // rthsEnableBufferValidation
        }
        if (!mesh_dxr->index_buffer) {
            if (mesh->gpu_index_buffer)
                mesh_dxr->index_buffer = translate_gpu_buffer(mesh->gpu_index_buffer);
            else if (mesh->cpu_index_buffer)
                mesh_dxr->index_buffer = upload_cpu_buffer(mesh->cpu_index_buffer, mesh->index_count * mesh->index_stride);

            if (!mesh_dxr->index_buffer->resource) {
                DebugPrint("GfxContextDXR::setMeshes(): failed to translate index buffer\n");
                continue;
            }

            if (mesh->index_stride == 0)
                mesh->index_stride = mesh_dxr->index_buffer->size / mesh->index_count;

#ifdef rthsEnableBufferValidation
            if (mesh_dxr->index_buffer) {
                // inspect buffer
                std::vector<uint32_t> index_buffer_data32;
                std::vector<uint16_t> index_buffer_data16;
                if (mesh_dxr->getIndexStride() == 2) {
                    index_buffer_data16.resize(mesh_dxr->index_buffer->size / sizeof(uint16_t), std::numeric_limits<uint16_t>::max());
                    readbackBuffer(rd, index_buffer_data16.data(), mesh_dxr->index_buffer->resource, mesh_dxr->index_buffer->size);
                }
                else {
                    index_buffer_data32.resize(mesh_dxr->index_buffer->size / sizeof(uint32_t), std::numeric_limits<uint32_t>::max());
                    readbackBuffer(rd, index_buffer_data32.data(), mesh_dxr->index_buffer->resource, mesh_dxr->index_buffer->size);
                }
            }
#endif // rthsEnableBufferValidation
        }

        auto& inst_dxr = m_meshinstance_records[inst];
        if (!inst_dxr) {
            inst_dxr = std::make_shared<MeshInstanceDataDXR>();
            inst_dxr->base = inst;
            inst_dxr->mesh = mesh_dxr;
        }
        if (gpu_skinning) {
            bool submit = rd.hasFlag(RenderFlag::ParallelCommandList) && deform_count > 0 && (deform_count % task_granularity) == 0;
            if (m_deformer->deform(rd, *inst_dxr, submit))
                ++deform_count;
        }
        rd.geometries.push_back({ inst_dxr, geom.receive_mask, geom.cast_mask });
    }
    if (gpu_skinning)
        m_deformer->flush(rd);


    // build BLAS
    size_t geometry_count = rd.geometries.size();
    auto cl_blas = rd.clm_blas->get();
    rthsTimestampQuery(rd.timestamp, cl_blas, "Building BLAS begin");
    int blas_update_count = 0;
    for (auto& geom_dxr : rd.geometries) {
        auto& inst_dxr = *geom_dxr.inst;
        auto& mesh_dxr = *inst_dxr.mesh;
        auto& inst = *inst_dxr.base;
        auto& mesh = *mesh_dxr.base;

        // note:
        // a instance can be processed multiple times in a frame (e.g. multiple renderers in the scene)
        // so, we need to make sure a BLAS is updated *once* in a frame, but TLAS in all renderers need to be updated if there is any updated object.
        // inst.update_flags indicates the object is updated, and is cleared immediately after processed.
        // inst_dxr.blas_updated keeps if blas is updated in the frame.

        auto queue_command = [&]() {
            bool submit = rd.hasFlag(RenderFlag::ParallelCommandList) && (blas_update_count % task_granularity) == 0;
            if (submit) {
                cl_blas->Close();
                cl_blas = rd.clm_blas->get();
            }
        };

        if (gpu_skinning && inst_dxr.deformed_vertices) {
            if (!inst_dxr.blas_deformed || inst.isUpdated(UpdateFlag::Any)) {
                // BLAS for deformable meshes

                bool perform_update = inst_dxr.blas_deformed != nullptr;

                D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
                geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

                geom_desc.Triangles.VertexBuffer.StartAddress = inst_dxr.deformed_vertices->GetGPUVirtualAddress();
                geom_desc.Triangles.VertexBuffer.StrideInBytes = sizeof(float4);
                geom_desc.Triangles.VertexCount = mesh.vertex_count;
                geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

                geom_desc.Triangles.IndexBuffer = mesh_dxr.index_buffer->resource->GetGPUVirtualAddress() + mesh.index_offset;
                geom_desc.Triangles.IndexCount = mesh.index_count;
                geom_desc.Triangles.IndexFormat = mesh.index_stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
                inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
                inputs.Flags =
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE |
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
                if (perform_update)
                    inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
                inputs.NumDescs = 1;
                inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
                inputs.pGeometryDescs = &geom_desc;

                if (!inst_dxr.blas_deformed) {
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
                    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

                    inst_dxr.blas_scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
                    inst_dxr.blas_deformed = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
                }

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
                as_desc.Inputs = inputs;
                if (perform_update)
                    as_desc.SourceAccelerationStructureData = inst_dxr.blas_deformed->GetGPUVirtualAddress();
                as_desc.DestAccelerationStructureData = inst_dxr.blas_deformed->GetGPUVirtualAddress();
                as_desc.ScratchAccelerationStructureData = inst_dxr.blas_scratch->GetGPUVirtualAddress();

                addResourceBarrier(cl_blas, inst_dxr.deformed_vertices, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                cl_blas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
                addResourceBarrier(cl_blas, inst_dxr.deformed_vertices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                inst_dxr.is_updated = true;
                ++blas_update_count;

                // queue command list if ParallelCommandList is enabled
                queue_command();
            }
        }
        else {
            if (!mesh_dxr.blas) {
                // BLAS for static meshes

                D3D12_RAYTRACING_GEOMETRY_DESC geom_desc{};
                geom_desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
                geom_desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

                geom_desc.Triangles.VertexBuffer.StartAddress = mesh_dxr.vertex_buffer->resource->GetGPUVirtualAddress() + mesh.vertex_offset;
                geom_desc.Triangles.VertexBuffer.StrideInBytes = mesh_dxr.getVertexStride();
                geom_desc.Triangles.VertexCount = mesh.vertex_count;
                geom_desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

                geom_desc.Triangles.IndexBuffer = mesh_dxr.index_buffer->resource->GetGPUVirtualAddress() + mesh.index_offset;
                geom_desc.Triangles.IndexCount = mesh.index_count;
                geom_desc.Triangles.IndexFormat = mesh.index_stride == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
                inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
                inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
                inputs.NumDescs = 1;
                inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
                inputs.pGeometryDescs = &geom_desc;

                {
                    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
                    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

                    mesh_dxr.blas_scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
                    mesh_dxr.blas = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
                }

                D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
                as_desc.Inputs = inputs;
                as_desc.DestAccelerationStructureData = mesh_dxr.blas->GetGPUVirtualAddress();
                as_desc.ScratchAccelerationStructureData = mesh_dxr.blas_scratch->GetGPUVirtualAddress();

                cl_blas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
                inst_dxr.is_updated = true;
                ++blas_update_count;

                // queue command list if ParallelCommandList is enabled
                queue_command();
            }
            else if (inst.isUpdated(UpdateFlag::Transform)) {
                // transform was updated. so TLAS needs to be updated.
                inst_dxr.is_updated = true;
            }
        }
        if (inst_dxr.is_updated)
            needs_build_tlas = true;
        inst.update_flags = 0; // prevent other renderers to build BLAS again
    }

#ifdef rthsEnableTimestamp
    if (rd.hasFlag(RenderFlag::ParallelCommandList) && rd.timestamp->isEnabled()) {
        // make sure all building BLAS commands are finished before timestamp is set
        auto fv = incrementFenceValue();
        m_cmd_queue_direct->Signal(getFence(), fv);
        m_cmd_queue_direct->Wait(getFence(), fv);
    }
#endif
    rthsTimestampQuery(rd.timestamp, cl_blas, "Building BLAS end");
    cl_blas->Close();
    rd.fv_blas = submitCommandList(rd.clm_blas->getCommandLists(), rd.fv_deform);

    if (!needs_build_tlas) {
        // if there are no BLAS updates, check geometry list is the same as last render.
        // if true, no TLAS update is needed.
        needs_build_tlas = rd.geometries != rd.geometries_prev;
    }


    // build TLAS
    auto cl_tlas = rd.clm_tlas->get();
    rthsTimestampQuery(rd.timestamp, cl_tlas, "Building TLAS begin");
    if (needs_build_tlas) {
        // get the size of the TLAS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = (UINT)geometry_count;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;


        // instance desc buffer
        ReuseOrExpandBuffer(rd.instance_desc, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), geometry_count, 4096, [this](size_t size) {
            return createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        });

        // create instance desc
        if (geometry_count > 0) {
            D3D12_RAYTRACING_INSTANCE_DESC *instance_descs;
            rd.instance_desc->Map(0, nullptr, (void**)&instance_descs);
            for (size_t i = 0; i < geometry_count; i++) {
                auto& geom_dxr = rd.geometries[i];
                auto& inst_dxr = *geom_dxr.inst;
                bool deformed = gpu_skinning && inst_dxr.deformed_vertices;
                auto& blas = deformed ? inst_dxr.blas_deformed : inst_dxr.mesh->blas;

                D3D12_RAYTRACING_INSTANCE_DESC tmp{};
                (float3x4&)tmp.Transform = to_float3x4(inst_dxr.base->transform);
                tmp.InstanceID = i; // This value will be exposed to the shader via InstanceID()
                tmp.InstanceMask = (geom_dxr.receive_mask & (uint8_t)HitMask::Receiver) | geom_dxr.cast_mask;
                tmp.InstanceContributionToHitGroupIndex = 0;
                tmp.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE; // D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE
                tmp.AccelerationStructure = blas->GetGPUVirtualAddress();
                instance_descs[i] = tmp;
            }
            rd.instance_desc->Unmap(0, nullptr);
            inputs.InstanceDescs = rd.instance_desc->GetGPUVirtualAddress();
        }

        // create TLAS
        {
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
            m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

            // scratch buffer
            ReuseOrExpandBuffer(rd.tlas_scratch, 1, info.ScratchDataSizeInBytes, 1024 * 64, [this, &rd](size_t size) {
                return createBuffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
            });

            // TLAS buffer
            bool expanded = ReuseOrExpandBuffer(rd.tlas, 1, info.ResultDataMaxSizeInBytes, 1024 * 256, [this, &rd](size_t size) {
                return createBuffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
            });
            if (expanded) {
                // SRV
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.RaytracingAccelerationStructure.Location = rd.tlas->GetGPUVirtualAddress();
                m_device->CreateShaderResourceView(nullptr, &srv_desc, rd.tlas_handle.hcpu);
            }

            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
            as_desc.DestAccelerationStructureData = rd.tlas->GetGPUVirtualAddress();
            as_desc.Inputs = inputs;
            if (rd.instance_desc)
                as_desc.Inputs.InstanceDescs = rd.instance_desc->GetGPUVirtualAddress();
            if (rd.tlas_scratch)
                as_desc.ScratchAccelerationStructureData = rd.tlas_scratch->GetGPUVirtualAddress();

            // build
            cl_tlas->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
        }

        // add UAV barrier
        {
            D3D12_RESOURCE_BARRIER uav_barrier{};
            uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uav_barrier.UAV.pResource = rd.tlas;
            cl_tlas->ResourceBarrier(1, &uav_barrier);
        }
    }
    rthsTimestampQuery(rd.timestamp, cl_tlas, "Building TLAS end");
    cl_tlas->Close();
    rd.fv_tlas = submitCommandList(rd.clm_tlas->getCommandLists(), rd.fv_blas);


    // setup per-instance data
    if (needs_build_tlas) {
        size_t stride = sizeof(InstanceData);
        bool expanded = ReuseOrExpandBuffer(rd.instance_data, stride, geometry_count, 4096, [this, &rd](size_t size) {
            return createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        });
        if (expanded) {
            auto capacity = rd.instance_data->GetDesc().Width;
            D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            desc.Buffer.FirstElement = 0;
            desc.Buffer.NumElements = UINT(capacity / stride);
            desc.Buffer.StructureByteStride = UINT(stride);
            desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            m_device->CreateShaderResourceView(rd.instance_data, &desc, rd.instance_data_handle.hcpu);
        }

        InstanceData *dst;
        if (SUCCEEDED(rd.instance_data->Map(0, nullptr, (void**)&dst))) {
            for (auto& geom_dxr : rd.geometries) {
                InstanceData tmp{};
                tmp.related_caster_mask = geom_dxr.receive_mask & (uint8_t)HitMask::AllCaster;
                *dst++ = tmp;
            }
            rd.instance_data->Unmap(0, nullptr);
        }
    }
}

void GfxContextDXR::flush(RenderDataDXR& rd)
{
    if (!valid())
        return;
    if (!rd.render_target || !rd.render_target->texture->resource) {
        SetErrorLog("GfxContext::flush(): render target is null\n");
        return;
    }
    if (rd.fv_rays != 0) {
        SetErrorLog("GfxContext::flush(): called before finish()\n");
        return;
    }
    auto& rtex = rd.render_target->texture;

    auto cl_rays = rd.clm_rays->get();
    rthsTimestampQuery(rd.timestamp, cl_rays, "DispatchRays begin");

    UINT64 shader_record_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    shader_record_size = align_to(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shader_record_size);

    // setup shader table
    if (!rd.shader_table) {
        const int capacity = 32;
        rd.shader_table = createBuffer(shader_record_size * capacity, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

        // setup shader table
        uint8_t *addr;
        rd.shader_table->Map(0, nullptr, (void**)&addr);

        ID3D12StateObjectPropertiesPtr sop;
        m_pipeline_state->QueryInterface(IID_PPV_ARGS(&sop));

        auto add_shader_record = [&](void *shader_id) {
            if (!shader_id)
                return;

            auto dst = addr;
            memcpy(dst, shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            dst += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

            addr += shader_record_size;
        };

        // ray-gen
        add_shader_record(sop->GetShaderIdentifier(kRayGenShader1));
        add_shader_record(sop->GetShaderIdentifier(kRayGenShader2));

        // miss
        add_shader_record(sop->GetShaderIdentifier(kMissShader1));
        add_shader_record(sop->GetShaderIdentifier(kMissShader2));

        // hit
        add_shader_record(sop->GetShaderIdentifier(kHitGroup1));
        add_shader_record(sop->GetShaderIdentifier(kHitGroup2));

        rd.shader_table->Unmap(0, nullptr);
    }


    // bind root signature and shader resources
    cl_rays->SetComputeRootSignature(m_rootsig);

    ID3D12DescriptorHeap *desc_heaps[] = { rd.desc_heap };
    cl_rays->SetDescriptorHeaps(_countof(desc_heaps), desc_heaps);


    auto do_dispatch_rays = [&](int raygen_index) {
        D3D12_DISPATCH_RAYS_DESC dr_desc{};
        dr_desc.Width = rtex->width;
        dr_desc.Height = rtex->height;
        dr_desc.Depth = 1;

        auto addr = rd.shader_table->GetGPUVirtualAddress();
        // ray-gen
        dr_desc.RayGenerationShaderRecord.StartAddress = addr + (shader_record_size * raygen_index);
        dr_desc.RayGenerationShaderRecord.SizeInBytes = shader_record_size;
        addr += shader_record_size * 2;

        // miss
        dr_desc.MissShaderTable.StartAddress = addr;
        dr_desc.MissShaderTable.StrideInBytes = shader_record_size;
        dr_desc.MissShaderTable.SizeInBytes = shader_record_size * 2;
        addr += dr_desc.MissShaderTable.SizeInBytes;

        // hit
        dr_desc.HitGroupTable.StartAddress = addr;
        dr_desc.HitGroupTable.StrideInBytes = shader_record_size;
        dr_desc.HitGroupTable.SizeInBytes = shader_record_size * 2;
        addr += dr_desc.HitGroupTable.SizeInBytes;

        cl_rays->SetPipelineState1(m_pipeline_state.GetInterfacePtr());
        cl_rays->DispatchRays(&dr_desc);
    };

    // dispatch
    if (rd.hasFlag(RenderFlag::AdaptiveSampling) && rd.render_target->quarter_res && rd.render_target->half_res) {
        // adaptive sampling
        addResourceBarrier(cl_rays, rd.render_target->quarter_res, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cl_rays->SetComputeRootDescriptorTable(0, rd.render_target_handles[2].hgpu);
        cl_rays->SetComputeRootDescriptorTable(1, rd.tlas_handle.hgpu);
        do_dispatch_rays(0);
        addResourceBarrier(cl_rays, rd.render_target->quarter_res, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

        addResourceBarrier(cl_rays, rd.render_target->half_res, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cl_rays->SetComputeRootDescriptorTable(0, rd.render_target_handles[1].hgpu);
        cl_rays->SetComputeRootDescriptorTable(2, rd.prev_result_handles[1].hgpu);
        do_dispatch_rays(1);
        addResourceBarrier(cl_rays, rd.render_target->half_res, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

        addResourceBarrier(cl_rays, rtex->resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cl_rays->SetComputeRootDescriptorTable(0, rd.render_target_handles[0].hgpu);
        cl_rays->SetComputeRootDescriptorTable(2, rd.prev_result_handles[0].hgpu);
        do_dispatch_rays(1);
        addResourceBarrier(cl_rays, rtex->resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
    }
    else {
        addResourceBarrier(cl_rays, rtex->resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cl_rays->SetComputeRootDescriptorTable(0, rd.render_target_handles[0].hgpu);
        cl_rays->SetComputeRootDescriptorTable(1, rd.tlas_handle.hgpu);
        do_dispatch_rays(0);
        addResourceBarrier(cl_rays, rtex->resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
    }


    rthsTimestampQuery(rd.timestamp, cl_rays, "DispatchRays end");
    rthsTimestampResolve(rd.timestamp, cl_rays);

    cl_rays->Close();
    rd.fv_rays = submitCommandList(rd.clm_rays->getCommandLists(), rd.fv_tlas);
    if (rd.fv_rays && rd.render_target && m_resource_translator) {
        // copy render target to Unity side
        auto fv = m_resource_translator->syncTexture(*rtex, rd.fv_rays);
        if (fv) {
            m_cmd_queue_direct->Wait(m_fence, fv);
            m_cmd_queue_direct->Signal(m_fence, ++fv);
            setFenceValue(fv);
            rd.fv_rays = fv;
        }
    }
    m_fv_last_rays = rd.fv_rays;
}

bool GfxContextDXR::finish(RenderDataDXR& rd)
{
    if (!valid())
        return false;

    if (rd.fv_rays != 0) {
        m_fence->SetEventOnCompletion(rd.fv_rays, rd.fence_event);
        ::WaitForSingleObject(rd.fence_event, INFINITE);
        rd.fv_rays = 0;

#ifdef rthsEnableRenderTargetValidation
        if (rd.render_target) {
            auto do_readback = [this, &rd](auto& tex, auto&& data) {
                data.resize(tex.width * tex.height, std::numeric_limits<float>::quiet_NaN());
                readbackTexture(rd, data.data(), tex.resource, tex.width, tex.height, tex.format);
                // break here to inspect data
            };

            auto& tex = *rd.render_target->texture;
            switch (GetTypedFormatDXR(tex.format)) {
            case DXGI_FORMAT_R8_UNORM: do_readback(tex, std::vector<unorm8>()); break;
            case DXGI_FORMAT_R16_FLOAT: do_readback(tex, std::vector<half>()); break;
            case DXGI_FORMAT_R32_FLOAT: do_readback(tex, std::vector<float>()); break;
            }
        }
#endif // rthsEnableRenderTargetValidation
    }

    if (!checkError()) {
        return false;
    }
    else {
        std::swap(rd.geometries, rd.geometries_prev);
        rd.geometries.clear();

        rthsTimestampUpdateLog(rd.timestamp, m_cmd_queue_direct);

        m_deformer->reset(rd);
        rd.clm_blas->reset();
        rd.clm_tlas->reset();
        rd.clm_rays->reset();
        rd.clm_immediate_copy->reset();
        return true;
    }
}

void GfxContextDXR::frameEnd()
{
    // erase unused texture / buffer resources
    auto erase_unused_records = [](auto& records, const char *message) {
        int num_erased = 0;
        for (auto it = records.begin(); it != records.end(); /**/) {
            if (it->second.use_count() == 1) {
                records.erase(it++);
                ++num_erased;
            }
            else {
                ++it;
            }
        }
        if (num_erased > 0) {
            DebugPrint(message, num_erased);
        }
    };
    erase_unused_records(m_texture_records, "GfxContextDXR::releaseUnusedResources(): erased %d texture records\n");
    erase_unused_records(m_buffer_records, "GfxContextDXR::releaseUnusedResources(): erased %d buffer records\n");
}

bool GfxContextDXR::readbackRenderTarget(RenderDataDXR& rd, void *dst)
{
    if (!rd.render_target || !rd.render_target->texture->resource)
        return false;

    auto& rtex = rd.render_target->texture;
    auto desc = rtex->resource->GetDesc();
    return readbackTexture(rd, dst, rtex->resource, (UINT)desc.Width, (UINT)desc.Height, desc.Format);
}

void GfxContextDXR::clearResourceCache()
{
    m_texture_records.clear();
    m_buffer_records.clear();
    m_mesh_records.clear();
    m_meshinstance_records.clear();
    m_rendertarget_records.clear();
}

void GfxContextDXR::onMeshDelete(MeshData *mesh)
{
    m_mesh_records.erase(mesh);
}

void GfxContextDXR::onMeshInstanceDelete(MeshInstanceData *mesh)
{
    m_meshinstance_records.erase(mesh);
}

void GfxContextDXR::onRenderTargetDelete(RenderTargetData *rt)
{
    m_rendertarget_records.erase(rt);
}


bool GfxContextDXR::valid() const
{
    return this && m_device && m_rootsig && m_pipeline_state;
}

bool GfxContextDXR::checkError()
{
    if (!m_device)
        return false;

    auto reason = m_device->GetDeviceRemovedReason();
    if (reason != 0) {
#ifdef rthsEnableD3D12DREAD
        {
            ID3D12DeviceRemovedExtendedDataPtr dread;
            if (SUCCEEDED(m_device->QueryInterface(IID_PPV_ARGS(&dread)))) {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumps;
                if (SUCCEEDED(dread->GetAutoBreadcrumbsOutput(&breadcrumps))) {
                    // todo: get error log
                }

                D3D12_DRED_PAGE_FAULT_OUTPUT pagefault;
                if (SUCCEEDED(dread->GetPageFaultAllocationOutput(&pagefault))) {
                    // todo: get error log
                }
            }
        }
#endif // rthsEnableD3D12DREAD

        {
            PSTR buf = nullptr;
            size_t size = ::FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, reason, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

            std::string message(buf, size);
            SetErrorLog(message.c_str());
        }

        clear();
        return false;
    }
    return true;
}

ID3D12Device5Ptr GfxContextDXR::getDevice() { return m_device; }
ID3D12CommandQueuePtr GfxContextDXR::getComputeQueue() { return m_cmd_queue_compute; }
ID3D12FencePtr GfxContextDXR::getFence() { return m_fence; }

uint64_t GfxContextDXR::incrementFenceValue()
{
    return ++m_fence_value;
}

void GfxContextDXR::setFenceValue(uint64_t v)
{
    m_fence_value = v;
}

ID3D12ResourcePtr GfxContextDXR::createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    ID3D12ResourcePtr ret;
    m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

ID3D12ResourcePtr GfxContextDXR::createTexture(int width, int height, DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    ID3D12ResourcePtr ret;
    auto hr = GfxContextDXR::getInstance()->getDevice()->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

void GfxContextDXR::addResourceBarrier(ID3D12GraphicsCommandList *cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    cl->ResourceBarrier(1, &barrier);
}

uint64_t GfxContextDXR::submitCommandList(ID3D12GraphicsCommandList *cl, uint64_t preceding_fv, bool emit_signal)
{
    ID3D12CommandList* cmd_list[]{ cl };
    return submitCommandList(cmd_list, 1, preceding_fv, emit_signal);
}

uint64_t GfxContextDXR::submitCommandList(const std::vector<ID3D12CommandList*>& cl, uint64_t preceding_fv, bool emit_signal)
{
    return submitCommandList(cl.data(), cl.size(), preceding_fv, emit_signal);
}

uint64_t GfxContextDXR::submitCommandList(ID3D12CommandList *const*cls, size_t n, uint64_t preceding_fv, bool emit_signal)
{
    if (preceding_fv != 0)
        m_cmd_queue_direct->Wait(m_fence, preceding_fv);

    m_cmd_queue_direct->ExecuteCommandLists((UINT)n, cls);

    if (emit_signal) {
        auto fence_value = incrementFenceValue();
        m_cmd_queue_direct->Signal(m_fence, fence_value);
        return fence_value;
    }
    else {
        return 0;
    }
}


bool GfxContextDXR::readbackBuffer(RenderDataDXR& rd, void *dst, ID3D12Resource *src, UINT64 size)
{
    auto cl = rd.clm_immediate_copy->get();
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    cl->CopyBufferRegion(readback_buf, 0, src, 0, size);
    executeImmediateCopy(rd, cl);

    float* mapped;
    if (SUCCEEDED(readback_buf->Map(0, nullptr, (void**)&mapped))) {
        memcpy(dst, mapped, size);
        // * break here to check data *
        readback_buf->Unmap(0, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::uploadBuffer(RenderDataDXR& rd, ID3D12Resource *dst, const void *src, UINT64 size)
{
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    void *mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, &mapped))) {
        memcpy(mapped, src, size);
        upload_buf->Unmap(0, nullptr);

        auto cl = rd.clm_immediate_copy->get();
        cl->CopyBufferRegion(dst, 0, upload_buf, 0, size);
        executeImmediateCopy(rd, cl);
        return true;
    }
    return false;
}

bool GfxContextDXR::readbackTexture(RenderDataDXR& rd, void *dst_, ID3D12Resource *src, UINT width, UINT height, DXGI_FORMAT format)
{
    UINT stride = SizeOfElement(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * stride;
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    if (!readback_buf)
        return false;

    D3D12_TEXTURE_COPY_LOCATION dst_loc{};
    dst_loc.pResource = readback_buf;
    dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst_loc.PlacedFootprint.Offset = 0;
    dst_loc.PlacedFootprint.Footprint.Format = GetTypelessFormatDXR(format);
    dst_loc.PlacedFootprint.Footprint.Width = width;
    dst_loc.PlacedFootprint.Footprint.Height = height;
    dst_loc.PlacedFootprint.Footprint.Depth = 1;
    dst_loc.PlacedFootprint.Footprint.RowPitch = width_a * stride;

    D3D12_TEXTURE_COPY_LOCATION src_loc{};
    src_loc.pResource = src;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_loc.SubresourceIndex = 0;

    auto cl = rd.clm_immediate_copy->get();
    cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    executeImmediateCopy(rd, cl);

    char *mapped;
    D3D12_RANGE ragne{ 0, size };
    if (SUCCEEDED(readback_buf->Map(0, &ragne, (void**)&mapped))) {
        auto dst = (char*)dst_;
        for (UINT yi = 0; yi < height; ++yi) {
            memcpy(dst, mapped, width * stride);
            dst += width * stride;
            mapped += width_a * stride;
        }
        readback_buf->Unmap(0, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::uploadTexture(RenderDataDXR& rd, ID3D12Resource *dst, const void *src_, UINT width, UINT height, DXGI_FORMAT format)
{
    UINT stride = SizeOfElement(format);
    UINT width_a = align_to(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT, width);
    UINT size = width_a * height * stride;
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    if (!upload_buf)
        return false;

    char *mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, (void**)&mapped))) {
        auto src = (const char*)src_;
        for (UINT yi = 0; yi < height; ++yi) {
            memcpy(mapped, src, width * stride);
            src += width * stride;
            mapped += width_a * stride;
        }
        upload_buf->Unmap(0, nullptr);

        D3D12_TEXTURE_COPY_LOCATION dst_loc{};
        dst_loc.pResource = dst;
        dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_loc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = upload_buf;
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_loc.PlacedFootprint.Offset = 0;
        src_loc.PlacedFootprint.Footprint.Format = GetTypelessFormatDXR(format);
        src_loc.PlacedFootprint.Footprint.Width = width;
        src_loc.PlacedFootprint.Footprint.Height = height;
        src_loc.PlacedFootprint.Footprint.Depth = 1;
        src_loc.PlacedFootprint.Footprint.RowPitch = width_a * stride;

        auto cl = rd.clm_immediate_copy->get();
        cl->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
        executeImmediateCopy(rd, cl);
        return true;
    }
    return false;
}

void GfxContextDXR::executeImmediateCopy(RenderDataDXR& rd, ID3D12GraphicsCommandList4Ptr& cl)
{
    cl->Close();
    ID3D12CommandList *cmd_list[]{ cl.GetInterfacePtr() };
    m_cmd_queue_immediate_copy->ExecuteCommandLists(_countof(cmd_list), cmd_list);

    auto fence_value = incrementFenceValue();
    m_cmd_queue_immediate_copy->Signal(m_fence, fence_value);
    m_fence->SetEventOnCompletion(fence_value, rd.fence_event);
    ::WaitForSingleObject(rd.fence_event, INFINITE);
}

} // namespace rths
#endif
