#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsResourceTranslatorDXR.h"
#include "rthsShaderDXR.h"


namespace rths {

static const WCHAR* kRayGenShader = L"rayGen";
static const WCHAR* kMissShader = L"miss";
static const WCHAR* kClosestHitShader = L"chs";
static const WCHAR* kHitGroup = L"HitGroup";

const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};

const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};

static dxc::DxcDllSupport gDxcDllHelper;

static ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc);

struct DxilLibrary
{
    DxilLibrary(ID3DBlobPtr pBlob, const WCHAR* entryPoint[], uint32_t entryPointCount) : pShaderBlob(pBlob)
    {
        stateSubobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        stateSubobject.pDesc = &dxilLibDesc;

        dxilLibDesc = {};
        exportDesc.resize(entryPointCount);
        exportName.resize(entryPointCount);
        if (pBlob)
        {
            dxilLibDesc.DXILLibrary.pShaderBytecode = pBlob->GetBufferPointer();
            dxilLibDesc.DXILLibrary.BytecodeLength = pBlob->GetBufferSize();
            dxilLibDesc.NumExports = entryPointCount;
            dxilLibDesc.pExports = exportDesc.data();

            for (uint32_t i = 0; i < entryPointCount; i++)
            {
                exportName[i] = entryPoint[i];
                exportDesc[i].Name = exportName[i].c_str();
                exportDesc[i].Flags = D3D12_EXPORT_FLAG_NONE;
                exportDesc[i].ExportToRename = nullptr;
            }
        }
    };

    DxilLibrary() : DxilLibrary(nullptr, nullptr, 0) {}

    D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
    D3D12_STATE_SUBOBJECT stateSubobject{};
    ID3DBlobPtr pShaderBlob;
    std::vector<D3D12_EXPORT_DESC> exportDesc;
    std::vector<std::wstring> exportName;
};

struct HitProgram
{
    HitProgram(LPCWSTR ahsExport, LPCWSTR chsExport, const std::wstring& name) : exportName(name)
    {
        desc = {};
        desc.AnyHitShaderImport = ahsExport;
        desc.ClosestHitShaderImport = chsExport;
        desc.HitGroupExport = exportName.c_str();

        subObject.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        subObject.pDesc = &desc;
    }

    std::wstring exportName;
    D3D12_HIT_GROUP_DESC desc;
    D3D12_STATE_SUBOBJECT subObject;
};

struct ExportAssociation
{
    ExportAssociation(const WCHAR* exportNames[], uint32_t exportCount, const D3D12_STATE_SUBOBJECT* pSubobjectToAssociate)
    {
        association.NumExports = exportCount;
        association.pExports = exportNames;
        association.pSubobjectToAssociate = pSubobjectToAssociate;

        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        subobject.pDesc = &association;
    }

    D3D12_STATE_SUBOBJECT subobject = {};
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION association = {};
};

struct LocalRootSignature
{
    LocalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
    {
        pRootSig = createRootSignature(pDevice, desc);
        pInterface = pRootSig.GetInterfacePtr();
        subobject.pDesc = &pInterface;
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    }
    ID3D12RootSignaturePtr pRootSig;
    ID3D12RootSignature* pInterface = nullptr;
    D3D12_STATE_SUBOBJECT subobject = {};
};

struct GlobalRootSignature
{
    GlobalRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
    {
        pRootSig = createRootSignature(pDevice, desc);
        pInterface = pRootSig.GetInterfacePtr();
        subobject.pDesc = &pInterface;
        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    }
    ID3D12RootSignaturePtr pRootSig;
    ID3D12RootSignature* pInterface = nullptr;
    D3D12_STATE_SUBOBJECT subobject = {};
};

struct ShaderConfig
{
    ShaderConfig(uint32_t maxAttributeSizeInBytes, uint32_t maxPayloadSizeInBytes)
    {
        shaderConfig.MaxAttributeSizeInBytes = maxAttributeSizeInBytes;
        shaderConfig.MaxPayloadSizeInBytes = maxPayloadSizeInBytes;

        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        subobject.pDesc = &shaderConfig;
    }

    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    D3D12_STATE_SUBOBJECT subobject = {};
};

struct PipelineConfig
{
    PipelineConfig(uint32_t maxTraceRecursionDepth)
    {
        config.MaxTraceRecursionDepth = maxTraceRecursionDepth;

        subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        subobject.pDesc = &config;
    }

    D3D12_RAYTRACING_PIPELINE_CONFIG config = {};
    D3D12_STATE_SUBOBJECT subobject = {};
};


template<class BlotType>
static inline std::string convertBlobToString(BlotType* pBlob)
{
    std::vector<char> infoLog(pBlob->GetBufferSize() + 1);
    memcpy(infoLog.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
    infoLog[pBlob->GetBufferSize()] = 0;
    return std::string(infoLog.data());
}

static ID3DBlobPtr compileLibrary(const char *shader, int shader_size, const WCHAR* targetString)
{
    // Initialize the helper
    gDxcDllHelper.Initialize();
    IDxcCompilerPtr pCompiler;
    IDxcLibraryPtr pLibrary;
    if (FAILED(gDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &pCompiler))) {
        SetErrorLog("failed to create compiler instance.\n");
        return nullptr;
    }
    if (FAILED(gDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary))) {
        SetErrorLog("failed to create dxc library instance.\n");
        return nullptr;
    }

    // Create blob from the string
    IDxcBlobEncodingPtr pTextBlob;
    pLibrary->CreateBlobWithEncodingFromPinned(shader, shader_size, 0, &pTextBlob);

    // Compile
    IDxcOperationResultPtr pResult;
    pCompiler->Compile(pTextBlob, nullptr, L"", targetString, nullptr, 0, nullptr, 0, nullptr, &pResult);

    // Verify the result
    HRESULT resultCode;
    pResult->GetStatus(&resultCode);
    if (FAILED(resultCode)) {
        IDxcBlobEncodingPtr pError;
        pResult->GetErrorBuffer(&pError);
        std::string log = convertBlobToString(pError.GetInterfacePtr());
        SetErrorLog("Compiler error: %s\n", log.c_str());
        return nullptr;
    }

    IDxcBlobPtr pBlob;
    pResult->GetResult(&pBlob);
    return pBlob;
}

static DxilLibrary createDxilLibrary(const char *shader, int shader_size)
{
    // Compile the shader
    ID3DBlobPtr pDxilLib = compileLibrary(shader, shader_size, L"lib_6_3");
    const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kClosestHitShader };
    return DxilLibrary(pDxilLib, entryPoints, arraysize(entryPoints));
}

static ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    ID3DBlobPtr pSigBlob;
    ID3DBlobPtr pErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
        SetErrorLog("%s\n", msg);
        return nullptr;
    }
    ID3D12RootSignaturePtr pRootSig;
    pDevice->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));
    return pRootSig;
}

struct RootSignatureDesc
{
    D3D12_ROOT_SIGNATURE_DESC desc = {};
    std::vector<D3D12_DESCRIPTOR_RANGE> range;
    std::vector<D3D12_ROOT_PARAMETER> rootParams;
};

static RootSignatureDesc createRayGenRootDesc()
{
    // Create the root-signature
    RootSignatureDesc desc;
    desc.range.resize(2);
    // gOutput
    desc.range[0].BaseShaderRegister = 0;
    desc.range[0].NumDescriptors = 1;
    desc.range[0].RegisterSpace = 0;
    desc.range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    desc.range[0].OffsetInDescriptorsFromTableStart = 0;

    // gRtScene
    desc.range[1].BaseShaderRegister = 0;
    desc.range[1].NumDescriptors = 1;
    desc.range[1].RegisterSpace = 0;
    desc.range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    desc.range[1].OffsetInDescriptorsFromTableStart = 1;

    desc.rootParams.resize(1);
    desc.rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    desc.rootParams[0].DescriptorTable.NumDescriptorRanges = 2;
    desc.rootParams[0].DescriptorTable.pDescriptorRanges = desc.range.data();

    // Create the desc
    desc.desc.NumParameters = 1;
    desc.desc.pParameters = desc.rootParams.data();
    desc.desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

    return desc;
}

static ID3D12DescriptorHeapPtr createDescriptorHeap(ID3D12Device5Ptr pDevice, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = count;
    desc.Type = type;
    desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    ID3D12DescriptorHeapPtr pHeap;
    pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap));
    return pHeap;
}


static const char* GetModulePath()
{
    static char s_path[MAX_PATH + 1];
    if (s_path[0] == 0) {
        HMODULE mod = 0;
        ::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)&GetModulePath, &mod);
        DWORD size = ::GetModuleFileNameA(mod, s_path, sizeof(s_path));
        for (int i = size - 1; i >= 0; --i) {
            if (s_path[i] == '\\') {
                s_path[i] = '\0';
                break;
            }
        }
    }
    return s_path;
}

static void AddDLLSearchPath(const char *v)
{
    std::string path;
    {
        DWORD size = ::GetEnvironmentVariableA("PATH", nullptr, 0);
        if (size > 0) {
            path.resize(size);
            ::GetEnvironmentVariableA("PATH", &path[0], (DWORD)path.size());
            path.pop_back(); // delete last '\0'
        }
    }
    if (path.find(v) == std::string::npos) {
        if (!path.empty()) { path += ";"; }
        auto pos = path.size();
        path += v;
        for (size_t i = pos; i < path.size(); ++i) {
            char& c = path[i];
            if (c == '/') { c = '\\'; }
        }
        ::SetEnvironmentVariableA("PATH", path.c_str());
    }
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
    AddDLLSearchPath(GetModulePath());

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

        D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
        hr = device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
        if (SUCCEEDED(hr) && features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
            m_device = device;
            break;
        }
    }

    if (!m_device) {
        SetErrorLog("DXR is not supported on this device");
    }
    else {
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

        {
            // pipeline state

            // Need 10 subobjects:
            //  1 for the DXIL library
            //  1 for hit-group
            //  2 for RayGen root-signature (root-signature and the subobject association)
            //  2 for the root-signature shared between miss and hit shaders (signature and association)
            //  2 for shader config (shared between all programs. 1 for the config, 1 for association)
            //  1 for pipeline config
            //  1 for the global root signature
            std::array<D3D12_STATE_SUBOBJECT, 10> subobjects;
            uint32_t index = 0;

            // Create the DXIL library
            DxilLibrary dxilLib = createDxilLibrary(rthsShaderDXR, rthsShaderDXR_size);
            subobjects[index++] = dxilLib.stateSubobject; // 0 Library

            HitProgram hitProgram(nullptr, kClosestHitShader, kHitGroup);
            subobjects[index++] = hitProgram.subObject; // 1 Hit Group

            // Create the ray-gen root-signature and association
            LocalRootSignature rgsRootSignature(m_device, createRayGenRootDesc().desc);
            subobjects[index] = rgsRootSignature.subobject; // 2 RayGen Root Sig

            uint32_t rgsRootIndex = index++; // 2
            ExportAssociation rgsRootAssociation(&kRayGenShader, 1, &(subobjects[rgsRootIndex]));
            subobjects[index++] = rgsRootAssociation.subobject; // 3 Associate Root Sig to RGS

            // Create the miss- and hit-programs root-signature and association
            D3D12_ROOT_SIGNATURE_DESC emptyDesc = {};
            emptyDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
            LocalRootSignature hitMissRootSignature(m_device, emptyDesc);
            subobjects[index] = hitMissRootSignature.subobject; // 4 Root Sig to be shared between Miss and CHS

            uint32_t hitMissRootIndex = index++; // 4
            const WCHAR* missHitExportName[] = { kMissShader, kClosestHitShader };
            ExportAssociation missHitRootAssociation(missHitExportName, arraysize(missHitExportName), &(subobjects[hitMissRootIndex]));
            subobjects[index++] = missHitRootAssociation.subobject; // 5 Associate Root Sig to Miss and CHS

            // Bind the payload size to the programs
            ShaderConfig shaderConfig(sizeof(float) * 2, sizeof(float) * 3);
            subobjects[index] = shaderConfig.subobject; // 6 Shader Config

            uint32_t shaderConfigIndex = index++; // 6
            const WCHAR* shaderExports[] = { kMissShader, kClosestHitShader, kRayGenShader };
            ExportAssociation configAssociation(shaderExports, arraysize(shaderExports), &(subobjects[shaderConfigIndex]));
            subobjects[index++] = configAssociation.subobject; // 7 Associate Shader Config to Miss, CHS, RGS

            // Create the pipeline config
            PipelineConfig config(1);
            subobjects[index++] = config.subobject; // 8

            // Create the global root signature and store the empty signature
            GlobalRootSignature root(m_device, {});
            m_empty_rootsig = root.pRootSig;
            subobjects[index++] = root.subobject; // 9

            // Create the state
            D3D12_STATE_OBJECT_DESC desc;
            desc.NumSubobjects = index; // 10
            desc.pSubobjects = subobjects.data();
            desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

            m_device->CreateStateObject(&desc, IID_PPV_ARGS(&m_pipeline_state));
        }

        {
            // shader table

            /** The shader-table layout is as follows:
                Entry 0 - Ray-gen program
                Entry 1 - Miss program
                Entry 2 - Hit program
                All entries in the shader-table must have the same size, so we will choose it base on the largest required entry.
                The ray-gen program requires the largest entry - sizeof(program identifier) + 8 bytes for a descriptor-table.
                The entry size must be aligned up to D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT
            */

            m_srv_uav_heap = createDescriptorHeap(m_device, 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

            // Calculate the size and create the buffer
            m_shader_table_entry_size = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
            m_shader_table_entry_size += 8; // The ray-gen's descriptor table
            m_shader_table_entry_size = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, m_shader_table_entry_size);
            uint32_t shader_table_size = m_shader_table_entry_size * 3;

            // For simplicity, we create the shader-table on the upload heap. You can also create it on the default heap
            m_shader_table = createBuffer(shader_table_size, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);

            // Map the buffer
            uint8_t* data;
            m_shader_table->Map(0, nullptr, (void**)&data);

            ID3D12StateObjectPropertiesPtr sop;
            m_pipeline_state->QueryInterface(IID_PPV_ARGS(&sop));

            // Entry 0 - ray-gen program ID and descriptor data
            memcpy(data, sop->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
            uint64_t heap_start = m_srv_uav_heap->GetGPUDescriptorHandleForHeapStart().ptr;
            *(uint64_t*)(data + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES) = heap_start;

            // Entry 1 - miss program
            memcpy(data + m_shader_table_entry_size, sop->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

            // Entry 2 - hit program
            uint8_t* hit_entry = data + m_shader_table_entry_size * 2; // +2 skips the ray-gen and miss entries
            memcpy(hit_entry, sop->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

            // Unmap
            m_shader_table->Unmap(0, nullptr);
        }
    }
}

GfxContextDXR::~GfxContextDXR()
{
}

ID3D12ResourcePtr GfxContextDXR::createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props)
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
    m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&ret));
    return ret;
}

void GfxContextDXR::setRenderTarget(TextureData rt)
{
    m_render_target = rt;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->CreateUnorderedAccessView(rt.resource, nullptr, &uav_desc, m_srv_uav_heap->GetCPUDescriptorHandleForHeapStart());
}

void GfxContextDXR::setMeshes(std::vector<MeshBuffers>& meshes)
{
    // build bottom level acceleration structures
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geom_descs;
    geom_descs.resize(meshes.size());
    size_t num_meshes = meshes.size();
    for (size_t i = 0; i < num_meshes; ++i) {
        auto& mesh = meshes[i];
        if (mesh.acceleration_structure)
            continue;

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
        // transform is handled by top level acceleration structures

        // Get the size requirements for the scratch and AS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = 1;
        inputs.pGeometryDescs = &geom_desc;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
        m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
        auto scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
        m_temporary_buffers.push_back(scratch);
        mesh.acceleration_structure = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

        // Create the bottom-level AS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc = {};
        as_desc.Inputs = inputs;
        as_desc.DestAccelerationStructureData = mesh.acceleration_structure->GetGPUVirtualAddress();
        as_desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();

        m_cmd_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        D3D12_RESOURCE_BARRIER uav_barrier = {};
        uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uav_barrier.UAV.pResource = mesh.acceleration_structure;
        m_cmd_list->ResourceBarrier(1, &uav_barrier);
    }


    // build top level acceleration structures
    {
        // First, get the size of the TLAS buffers and create them
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = 3;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
        m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

        // Create the buffers
        auto scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
        m_temporary_buffers.push_back(scratch);
        m_toplevel_as = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
        uint64_t tlas_size = info.ResultDataMaxSizeInBytes;

        // The instance desc should be inside a buffer, create and map the buffer
        auto instance_descs_buf = createBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_meshes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
        m_temporary_buffers.push_back(instance_descs_buf);
        D3D12_RAYTRACING_INSTANCE_DESC* instance_descs;
        instance_descs_buf->Map(0, nullptr, (void**)&instance_descs);
        ZeroMemory(instance_descs, sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * num_meshes);
        for (uint32_t i = 0; i < num_meshes; i++) {
            auto& mesh = meshes[i];

            (float3x4&)instance_descs[i].Transform = mesh.transform;
            instance_descs[i].InstanceID = i; // This value will be exposed to the shader via InstanceID()
            instance_descs[i].InstanceContributionToHitGroupIndex = i;
            instance_descs[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE; // D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE
            instance_descs[i].AccelerationStructure = mesh.acceleration_structure->GetGPUVirtualAddress();
            instance_descs[i].InstanceMask = 0xFF;
        }
        instance_descs_buf->Unmap(0, nullptr);

        // Create the TLAS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC as_desc = {};
        as_desc.Inputs = inputs;
        as_desc.Inputs.InstanceDescs = instance_descs_buf->GetGPUVirtualAddress();
        as_desc.DestAccelerationStructureData = m_toplevel_as->GetGPUVirtualAddress();
        as_desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();

        m_cmd_list->BuildRaytracingAccelerationStructure(&as_desc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        D3D12_RESOURCE_BARRIER uav_barrier = {};
        uav_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uav_barrier.UAV.pResource = m_toplevel_as;
        m_cmd_list->ResourceBarrier(1, &uav_barrier);
    }
}


bool GfxContextDXR::valid() const
{
    return m_device != nullptr;
}

ID3D12Device5* GfxContextDXR::getDevice()
{
    return m_device;
}

TextureData GfxContextDXR::translateTexture(void *ptr)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->createTemporaryRenderTarget(ptr);
    return {};
}

void GfxContextDXR::copyTexture(void *dst, ID3D12ResourcePtr src)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->copyTexture(dst, src);
}

BufferData GfxContextDXR::translateVertexBuffer(void *ptr)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->translateVertexBuffer(ptr);
    return {};
}

BufferData GfxContextDXR::translateIndexBuffer(void *ptr)
{
    if (auto translator = GetResourceTranslator(m_device))
        return translator->translateIndexBuffer(ptr);
    return {};
}

BufferData GfxContextDXR::allocateTransformBuffer(const float4x4& trans)
{
    BufferData ret;
    ret.size = sizeof(float) * 12;
    ret.resource = createBuffer(ret.size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    return ret;
}

void GfxContextDXR::addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier = {};
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

void GfxContextDXR::flush()
{
    if (!m_render_target.resource) {
        SetErrorLog("GfxContext::flush(): render target is null\n");
        return;
    }

    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_DISPATCH_RAYS_DESC dr_desc = {};
    dr_desc.Width = m_render_target.width;
    dr_desc.Height = m_render_target.height;
    dr_desc.Depth = 1;

    // RayGen is the first entry in the shader-table
    dr_desc.RayGenerationShaderRecord.StartAddress = m_shader_table->GetGPUVirtualAddress() + 0 * m_shader_table_entry_size;
    dr_desc.RayGenerationShaderRecord.SizeInBytes = m_shader_table_entry_size;

    // Miss is the second entry in the shader-table
    size_t miss_offset = 1 * m_shader_table_entry_size;
    dr_desc.MissShaderTable.StartAddress = m_shader_table->GetGPUVirtualAddress() + miss_offset;
    dr_desc.MissShaderTable.StrideInBytes = m_shader_table_entry_size;
    dr_desc.MissShaderTable.SizeInBytes = m_shader_table_entry_size;   // Only a s single miss-entry

    // Hit is the third entry in the shader-table
    size_t hit_offset = 2 * m_shader_table_entry_size;
    dr_desc.HitGroupTable.StartAddress = m_shader_table->GetGPUVirtualAddress() + hit_offset;
    dr_desc.HitGroupTable.StrideInBytes = m_shader_table_entry_size;
    dr_desc.HitGroupTable.SizeInBytes = m_shader_table_entry_size;

    // Bind the empty root signature
    m_cmd_list->SetComputeRootSignature(m_empty_rootsig);

    // Dispatch
    m_cmd_list->SetPipelineState1(m_pipeline_state.GetInterfacePtr());
    m_cmd_list->DispatchRays(&dr_desc);

    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    submitCommandList();
}

void GfxContextDXR::finish()
{
    ::WaitForSingleObject(m_fence_event, INFINITE);
    m_cmd_allocator->Reset();
    m_cmd_list->Reset(m_cmd_allocator, nullptr);

    m_temporary_buffers.clear();
}

} // namespace rths


// Unity plugin load event
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
    using namespace rths;
    GfxContextDXR::initializeInstance();

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
#endif
