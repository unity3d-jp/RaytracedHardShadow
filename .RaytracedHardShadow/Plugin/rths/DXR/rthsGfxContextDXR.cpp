#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsResourceTranslatorDXR.h"

// shader binaries
#include "rthsShadowDXR.h"


namespace rths {

extern ID3D12Device *g_host_d3d12_device;

static const WCHAR* kRayGenShader = L"RayGen";
static const WCHAR* kMissShader = L"Miss";
static const WCHAR* kAnyHitShader = L"AnyHit";
static const WCHAR* kClosestHitShader = L"ClosestHit";
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
    if (!initializeDevice())
        return;
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

ID3D12ResourcePtr GfxContextDXR::createTexture(int width, int height, DXGI_FORMAT format)
{
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE;
    D3D12_RESOURCE_STATES initial_state = D3D12_RESOURCE_STATE_COMMON;

    ID3D12ResourcePtr ret;
    auto hr = GfxContextDXR::getInstance()->getDevice()->CreateCommittedResource(&kDefaultHeapProps, flags, &desc, initial_state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

bool GfxContextDXR::initializeDevice()
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

    // resource translator
    m_resource_translator = CreateResourceTranslator();
    if (!m_resource_translator)
        return false;

    // fence
    m_fence = m_resource_translator->getFence(m_device);

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

    // local root signature
    {
        D3D12_DESCRIPTOR_RANGE ranges[] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 2 },
        };

        D3D12_ROOT_PARAMETER params[_countof(ranges)]{};
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
            { kAnyHitShader,     nullptr, D3D12_EXPORT_FLAG_NONE },
            { kClosestHitShader, nullptr, D3D12_EXPORT_FLAG_NONE },
        };
        LPCWSTR exports[] = { kRayGenShader, kMissShader, kAnyHitShader, kHitGroup };

        D3D12_DXIL_LIBRARY_DESC dxil_desc{};
        dxil_desc.DXILLibrary.pShaderBytecode = rthsShadowDXR;
        dxil_desc.DXILLibrary.BytecodeLength = sizeof(rthsShadowDXR);
        dxil_desc.NumExports = _countof(export_descs);
        dxil_desc.pExports = export_descs;
        add_subobject(D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &dxil_desc);

        D3D12_HIT_GROUP_DESC hit_desc{};
        hit_desc.HitGroupExport = kHitGroup;
        hit_desc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
        hit_desc.AnyHitShaderImport = kAnyHitShader;
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

void GfxContextDXR::frameBegin()
{
    for (auto& kvp : m_meshinstance_records) {
        kvp.second->is_updated = false;
    }
}

void GfxContextDXR::prepare(RenderDataDXR& rd)
{
    TimestampInitialize(rd.timestamp, m_device);
    TimestampReset(rd.timestamp);

    if (!rd.cmd_list_direct) {
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&rd.cmd_allocator_direct));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, rd.cmd_allocator_direct, nullptr, IID_PPV_ARGS(&rd.cmd_list_direct));
        rd.cmd_list_direct->Close();
    }
    if (!rd.cmd_list_immediate_copy) {
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&rd.cmd_allocator_immediate_copy));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, rd.cmd_allocator_immediate_copy, nullptr, IID_PPV_ARGS(&rd.cmd_list_immediate_copy));
    }
    rd.cmd_allocator_direct->Reset();
    rd.cmd_list_direct->Reset(rd.cmd_allocator_direct, nullptr);
    rd.fence_value = 0;

    // desc heap
    if (!rd.desc_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = 16;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rd.desc_heap));

        auto handle_allocator = DescriptorHeapAllocatorDXR(m_device, rd.desc_heap);
        rd.render_target_handle = handle_allocator.allocate();
        rd.tlas_handle = handle_allocator.allocate();
        rd.scene_data_handle = handle_allocator.allocate();
    }

    // scene constant buffer
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
}

void GfxContextDXR::setSceneData(RenderDataDXR& rd, SceneData& data)
{
    if (rd.scene_data_prev == data)
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

void GfxContextDXR::setRenderTarget(RenderDataDXR& rd, TextureData& rt)
{
    auto& data = m_texture_records[rt];
    if (!data) {
        data = m_resource_translator->createTemporaryTexture(rt);
        if (!data->resource) {
            DebugPrint("GfxContextDXR::setRenderTarget(): failed to translate texture\n");
            return;
        }
    }

    if (rd.render_target != data) {
        rd.render_target = data;

        if (rd.render_target) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uav_desc.Format = GetTypedFormatDXR(rd.render_target->format); // typeless is not allowed for unordered access view
            uav_desc.Texture2D.MipSlice = 0;
            uav_desc.Texture2D.PlaneSlice = 0;
            m_device->CreateUnorderedAccessView(rd.render_target->resource, nullptr, &uav_desc, rd.render_target_handle.hcpu);
        }
    }

#ifdef rthsEnableRenderTargetValidation
    if (rd.render_target) {
        // fill texture with 0.0-1.0 gradation for debug

        int n = rd.render_target->width * rd.render_target->height;
        float r = 1.0f / (float)n;

        std::vector<float> data;
        data.resize(n);
        for (int i = 0; i < n; ++i)
            data[i] = r * (float)i;
        uploadTexture(rd, rd.render_target->resource, data.data(), rd.render_target->width, rd.render_target->height, sizeof(float));
    }
#endif // rthsEnableRenderTargetValidation
}

void GfxContextDXR::setMeshes(RenderDataDXR& rd, std::vector<MeshInstanceData*>& instances)
{
    auto translate_buffer = [this](void *buffer) {
        auto& data = m_buffer_records[buffer];
        if (!data)
            data = m_resource_translator->translateBuffer(buffer);
        return data;
    };

    bool gpu_skinning = (rd.render_flags & (int)RenderFlag::GPUSkinning) != 0;
    int deform_count = 0;
    if (gpu_skinning)
        m_deformer->prepare(rd);
    rd.mesh_instances.clear();

    bool needs_build_tlas = false;
    for (auto& inst : instances) {
        auto& mesh = inst->mesh;
        if (mesh->vertex_count == 0 || mesh->index_count == 0)
            continue;

        auto& mesh_dxr = m_mesh_records[mesh];
        if (!mesh_dxr) {
            mesh_dxr = std::make_shared<MeshDataDXR>();
            mesh_dxr->base = mesh;
        }

        if (!mesh_dxr->vertex_buffer) {
            mesh_dxr->vertex_buffer = translate_buffer(mesh->vertex_buffer);
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
            mesh_dxr->index_buffer = translate_buffer(mesh->index_buffer);
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
            if (m_deformer->deform(rd, *inst_dxr))
                ++deform_count;
        }
        rd.mesh_instances.push_back(inst_dxr);
    }

    if (gpu_skinning) {
        m_deformer->finish(rd);

        auto fence_value = m_resource_translator->inclementFenceValue();
        auto cl = rd.cmd_list_compute;

        ID3D12CommandList* cmd_list[] = { cl.GetInterfacePtr() };
        m_cmd_queue_compute->ExecuteCommandLists(_countof(cmd_list), cmd_list);
        m_cmd_queue_compute->Signal(m_fence, fence_value);

        // add wait command because building acceleration structure depends on deform
        m_cmd_queue_direct->Wait(m_fence, fence_value);
        m_resource_translator->setFenceValue(fence_value);
    }

    TimestampQuery(rd.timestamp, rd.cmd_list_direct, "GfxContextDXR: building BLAS begin");

    // build bottom level acceleration structures
    for (auto & pinst_dxr : rd.mesh_instances) {
        auto& inst_dxr = *pinst_dxr;
        auto& mesh_dxr = *inst_dxr.mesh;
        auto& inst = *inst_dxr.base;
        auto& mesh = *mesh_dxr.base;

        // note:
        // a instance can be processed multiple times in a frame (e.g. multiple renderers in the scene)
        // so, we need to make sure a BLAS is updated *once* in a frame, but TLAS in all renderers need to be updated if there is any updated object.
        // inst.update_flags indicates the object is updated, and is cleared immediately after processed.
        // inst_dxr.blas_updated keeps if blas is updated in the frame.

        if (gpu_skinning && inst_dxr.deformed_vertices) {
            if (!inst_dxr.blas_deformed || inst.update_flags) {
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

                addResourceBarrier(rd.cmd_list_direct, inst_dxr.deformed_vertices, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                rd.cmd_list_direct->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
                addResourceBarrier(rd.cmd_list_direct, inst_dxr.deformed_vertices, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                inst_dxr.is_updated = true;
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

                rd.cmd_list_direct->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
                inst_dxr.is_updated = true;
            }
            else if (inst.update_flags)
                inst_dxr.is_updated = true;
        }
        if (inst_dxr.is_updated)
            needs_build_tlas = true;
        inst.update_flags = 0;
    }
    TimestampQuery(rd.timestamp, rd.cmd_list_direct, "GfxContextDXR: building BLAS end");

    TimestampQuery(rd.timestamp, rd.cmd_list_direct, "GfxContextDXR: building TLAS begin");

    if (!needs_build_tlas)
        needs_build_tlas = rd.mesh_instances != rd.mesh_instances_prev;

    // build top level acceleration structure
    if (needs_build_tlas) {
        size_t instance_count = rd.mesh_instances.size();

        // get the size of the TLAS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = (UINT)instance_count;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};
        m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        // scratch buffer
        if (rd.tlas_scratch) {
            auto capacity = rd.tlas_scratch->GetDesc().Width;
            if (capacity < info.ScratchDataSizeInBytes)
                rd.tlas_scratch = nullptr;
        }
        if (!rd.tlas_scratch) {
            rd.tlas_scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
        }

        // TLAS buffer
        if(rd.tlas) {
            auto capacity = rd.tlas->GetDesc().Width;
            if (capacity < info.ScratchDataSizeInBytes)
                rd.tlas = nullptr;
        }
        if (!rd.tlas) {
            rd.tlas = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

            // TLAS SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.RaytracingAccelerationStructure.Location = rd.tlas->GetGPUVirtualAddress();
            m_device->CreateShaderResourceView(nullptr, &srv_desc, rd.tlas_handle.hcpu);
        }

        // instance desc buffer
        if (rd.instance_desc) {
            auto capacity = rd.instance_desc->GetDesc().Width;
            if (capacity < instance_count * sizeof(D3D12_RAYTRACING_INSTANCE_DESC))
                rd.instance_desc = nullptr;
        }
        if (!rd.instance_desc) {
            size_t capacity = 1024;
            while (capacity < instance_count)
                capacity *= 2;
            rd.instance_desc = createBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * capacity, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        }

        // create instance desc
        if (instance_count > 0) {
            D3D12_RAYTRACING_INSTANCE_DESC *instance_descs;
            rd.instance_desc->Map(0, nullptr, (void**)&instance_descs);
            for (size_t i = 0; i < instance_count; i++) {
                auto& inst_dxr = *rd.mesh_instances[i];
                bool deformed = gpu_skinning && inst_dxr.deformed_vertices;
                auto& blas = deformed ? inst_dxr.blas_deformed : inst_dxr.mesh->blas;

                D3D12_RAYTRACING_INSTANCE_DESC tmp{};
                (float3x4&)tmp.Transform = to_float3x4(deformed ? float4x4::identity() : inst_dxr.base->transform);
                tmp.InstanceID = i; // This value will be exposed to the shader via InstanceID()
                tmp.InstanceMask = 0xFF;
                tmp.InstanceContributionToHitGroupIndex = i;
                tmp.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE; // D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE
                tmp.AccelerationStructure = blas->GetGPUVirtualAddress();
                instance_descs[i] = tmp;
            }
            rd.instance_desc->Unmap(0, nullptr);
            inputs.InstanceDescs = rd.instance_desc->GetGPUVirtualAddress();
        }

        // create TLAS
        {
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc{};
            as_desc.DestAccelerationStructureData = rd.tlas->GetGPUVirtualAddress();
            as_desc.Inputs = inputs;
            if (rd.instance_desc)
                as_desc.Inputs.InstanceDescs = rd.instance_desc->GetGPUVirtualAddress();
            if (rd.tlas_scratch)
                as_desc.ScratchAccelerationStructureData = rd.tlas_scratch->GetGPUVirtualAddress();

            rd.cmd_list_direct->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);
        }

        // add UAV barrier
        {
            D3D12_RESOURCE_BARRIER uav_barrier{};
            uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            uav_barrier.UAV.pResource = rd.tlas;
            rd.cmd_list_direct->ResourceBarrier(1, &uav_barrier);
        }
    }
    TimestampQuery(rd.timestamp, rd.cmd_list_direct, "GfxContextDXR: building TLAS end");
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
#endif // rthsEnableD3D12DREAD

        PSTR buf = nullptr;
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, reason, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

        std::string message(buf, size);
        SetErrorLog(message.c_str());
        return false;
    }
    return true;
}

ID3D12Device5Ptr GfxContextDXR::getDevice() { return m_device; }


void GfxContextDXR::addResourceBarrier(ID3D12GraphicsCommandList4Ptr cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    cl->ResourceBarrier(1, &barrier);
}

uint64_t GfxContextDXR::submitCommandList(ID3D12GraphicsCommandList4Ptr cl)
{
    cl->Close();
    ID3D12CommandList* cmd_list[]{ cl.GetInterfacePtr() };
    m_cmd_queue_direct->ExecuteCommandLists(_countof(cmd_list), cmd_list);

    auto fence_value = m_resource_translator->inclementFenceValue();
    m_cmd_queue_direct->Signal(m_fence, fence_value);
    return fence_value;
}


bool GfxContextDXR::readbackBuffer(RenderDataDXR& rd, void *dst, ID3D12Resource *src, size_t size)
{
    auto readback_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, kReadbackHeapProps);
    rd.cmd_list_immediate_copy->CopyBufferRegion(readback_buf, 0, src, 0, size);
    executeImmediateCopy(rd);

    float* mapped;
    if (SUCCEEDED(readback_buf->Map(0, nullptr, (void**)&mapped))) {
        memcpy(dst, mapped, size);
        // * break here to check data *
        readback_buf->Unmap(0, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::uploadBuffer(RenderDataDXR& rd, ID3D12Resource *dst, const void *src, size_t size)
{
    auto upload_buf = createBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    void *mapped;
    if (SUCCEEDED(upload_buf->Map(0, nullptr, &mapped))) {
        memcpy(mapped, src, size);
        upload_buf->Unmap(0, nullptr);

        rd.cmd_list_immediate_copy->CopyBufferRegion(dst, 0, upload_buf, 0, size);
        executeImmediateCopy(rd);
        return true;
    }
    return false;
}

bool GfxContextDXR::readbackTexture(RenderDataDXR& rd, void *dst, ID3D12Resource *src, size_t width, size_t height, size_t stride)
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
    src_loc.pResource = src;
    src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_loc.SubresourceIndex = 0;

    rd.cmd_list_immediate_copy->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
    executeImmediateCopy(rd);

    float* mapped;
    if (SUCCEEDED(readback_buf->Map(0, nullptr, (void**)&mapped))) {
        memcpy(dst, mapped, size);
        readback_buf->Unmap(0, nullptr);
        return true;
    }
    return false;
}

bool GfxContextDXR::uploadTexture(RenderDataDXR& rd, ID3D12Resource *dst, const void *src, size_t width, size_t height, size_t stride)
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
        dst_loc.pResource = dst;
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

        rd.cmd_list_immediate_copy->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
        executeImmediateCopy(rd);
        return true;
    }
    return false;
}

void GfxContextDXR::executeImmediateCopy(RenderDataDXR& rd)
{
    auto& command_list = rd.cmd_list_immediate_copy;
    auto& allocator = rd.cmd_allocator_immediate_copy;

    command_list->Close();
    ID3D12CommandList *cmd_list[]{ command_list.GetInterfacePtr() };
    m_cmd_queue_immediate_copy->ExecuteCommandLists(_countof(cmd_list), cmd_list);

    auto fence_value = m_resource_translator->inclementFenceValue();
    m_cmd_queue_immediate_copy->Signal(m_fence, fence_value);
    m_fence->SetEventOnCompletion(fence_value, rd.fence_event);
    ::WaitForSingleObject(rd.fence_event, INFINITE);

    allocator->Reset();
    command_list->Reset(allocator, nullptr);
}


uint64_t GfxContextDXR::flush(RenderDataDXR& rd)
{
    if (!rd.render_target->resource) {
        SetErrorLog("GfxContext::flush(): render target is null\n");
        return 0;
    }
    if (rd.fence_value != 0) {
        SetErrorLog("GfxContext::flush(): called before finish()\n");
        return 0;
    }

    TimestampQuery(rd.timestamp, rd.cmd_list_direct, "GfxContextDXR: raytrace begin");

    size_t shader_record_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    shader_record_size += sizeof(D3D12_GPU_DESCRIPTOR_HANDLE);
    shader_record_size = align_to(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, shader_record_size);

    size_t instance_count = rd.mesh_instances.size();

    // setup shader table
    {
        // ray-gen + miss + hit for each meshes
        size_t required_count = 2 + instance_count;

        if (rd.shader_table) {
            auto capacity = rd.shader_table->GetDesc().Width;
            if (capacity < required_count * shader_record_size)
                rd.shader_table = nullptr;
        }
        if (!rd.shader_table) {
            size_t capacity = 1024;
            while (capacity < required_count)
                capacity *= 2;
            rd.shader_table = createBuffer(shader_record_size * capacity, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

            // setup shader table
            {
                uint8_t *data;
                rd.shader_table->Map(0, nullptr, (void**)&data);

                ID3D12StateObjectPropertiesPtr sop;
                m_pipeline_state->QueryInterface(IID_PPV_ARGS(&sop));

                auto add_shader_table = [&](void *shader_id) {
                    auto dst = data;
                    memcpy(dst, shader_id, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                    dst += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
                    *(UINT64*)(dst) = rd.desc_heap->GetGPUDescriptorHandleForHeapStart().ptr;

                    data += shader_record_size;
                };

                // ray-gen
                add_shader_table(sop->GetShaderIdentifier(kRayGenShader));

                // miss
                add_shader_table(sop->GetShaderIdentifier(kMissShader));

                // hit for each meshes
                void *hit = sop->GetShaderIdentifier(kHitGroup);
                for (int i = 0; i < capacity - 2; ++i)
                    add_shader_table(hit);

                rd.shader_table->Unmap(0, nullptr);
            }
        }
    }

    D3D12_RESOURCE_STATES prev_state = D3D12_RESOURCE_STATE_COMMON;
    addResourceBarrier(rd.cmd_list_direct, rd.render_target->resource, prev_state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // dispatch rays
    {
        D3D12_DISPATCH_RAYS_DESC dr_desc{};
        dr_desc.Width = rd.render_target->width;
        dr_desc.Height = rd.render_target->height;
        dr_desc.Depth = 1;

        auto addr = rd.shader_table->GetGPUVirtualAddress();
        // ray-gen
        dr_desc.RayGenerationShaderRecord.StartAddress = addr;
        dr_desc.RayGenerationShaderRecord.SizeInBytes = shader_record_size;
        addr += shader_record_size;

        // miss
        dr_desc.MissShaderTable.StartAddress = addr;
        dr_desc.MissShaderTable.StrideInBytes = shader_record_size;
        dr_desc.MissShaderTable.SizeInBytes = shader_record_size; 
        addr += shader_record_size;

        // hit for each meshes
        dr_desc.HitGroupTable.StartAddress = addr;
        dr_desc.HitGroupTable.StrideInBytes = shader_record_size;
        dr_desc.HitGroupTable.SizeInBytes = shader_record_size * instance_count;

        // descriptor heaps
        ID3D12DescriptorHeap *desc_heaps[] = { rd.desc_heap };
        rd.cmd_list_direct->SetDescriptorHeaps(_countof(desc_heaps), desc_heaps);

        // bind root signature and shader resources
        rd.cmd_list_direct->SetComputeRootSignature(m_global_rootsig);

        // dispatch
        rd.cmd_list_direct->SetPipelineState1(m_pipeline_state.GetInterfacePtr());
        rd.cmd_list_direct->DispatchRays(&dr_desc);
    }

    addResourceBarrier(rd.cmd_list_direct, rd.render_target->resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, prev_state);
    TimestampQuery(rd.timestamp, rd.cmd_list_direct, "GfxContextDXR: raytrace end");
    TimestampResolve(rd.timestamp, rd.cmd_list_direct);

    rd.fence_value = submitCommandList(rd.cmd_list_direct);
    m_fence->SetEventOnCompletion(rd.fence_value, rd.fence_event);
    return rd.fence_value;
}

void GfxContextDXR::finish(RenderDataDXR& rd)
{
    if (rd.fence_value == 0) {
        return;
    }

    ::WaitForSingleObject(rd.fence_event, INFINITE);
    rd.fence_value = 0;

#ifdef rthsEnableRenderTargetValidation
    {
        std::vector<float> data;
        data.resize(rd.render_target->width * rd.render_target->height, std::numeric_limits<float>::quiet_NaN());
        readbackTexture(rd, data.data(), rd.render_target->resource, rd.render_target->width, rd.render_target->height, sizeof(float));
        // break here to inspect data
    }
#endif // rthsEnableRenderTargetValidation

    if (rd.render_target) {
        // copy content of render target to Unity side
        m_resource_translator->applyTexture(*rd.render_target);
    }
    std::swap(rd.mesh_instances, rd.mesh_instances_prev);
    rd.mesh_instances.clear();

    TimestampPrint(rd.timestamp, m_cmd_queue_direct);
}

void GfxContextDXR::frameEnd()
{
    // erase unused texture / buffer / mesh data
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

void GfxContextDXR::onMeshDelete(MeshData *mesh)
{
    m_mesh_records.erase(mesh);
}

void GfxContextDXR::onMeshInstanceDelete(MeshInstanceData *mesh)
{
    m_meshinstance_records.erase(mesh);
}

} // namespace rths
#endif
