#include "pch.h"
#include "rthsGfxContext.h"
#include "rthsResourceTranslator.h"

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)
#define arraysize(a) (sizeof(a)/sizeof(a[0]))

namespace rths {

static const WCHAR* kRayGenShader = L"rayGen";
static const WCHAR* kMissShader = L"miss";
static const WCHAR* kClosestHitShader = L"chs";
static const WCHAR* kHitGroup = L"HitGroup";

static const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
    D3D12_HEAP_TYPE_UPLOAD,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0,
};

static const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
    D3D12_HEAP_TYPE_DEFAULT,
    D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN,
    0,
    0
};

static dxc::DxcDllSupport gDxcDllHelper;

static ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc);
static ID3DBlobPtr compileLibrary(const WCHAR* filename, const WCHAR* targetString);

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


static DxilLibrary createDxilLibrary(const wchar_t *path_to_shader)
{
    // Compile the shader
    ID3DBlobPtr pDxilLib = compileLibrary(path_to_shader, L"lib_6_3");
    const WCHAR* entryPoints[] = { kRayGenShader, kMissShader, kClosestHitShader };
    return DxilLibrary(pDxilLib, entryPoints, arraysize(entryPoints));
}

template<class BlotType>
static inline std::string convertBlobToString(BlotType* pBlob)
{
    std::vector<char> infoLog(pBlob->GetBufferSize() + 1);
    memcpy(infoLog.data(), pBlob->GetBufferPointer(), pBlob->GetBufferSize());
    infoLog[pBlob->GetBufferSize()] = 0;
    return std::string(infoLog.data());
}

static std::wstring string_2_wstring(const std::string& s)
{
    std::wstring_convert<std::codecvt_utf8<WCHAR>> cvt;
    std::wstring ws = cvt.from_bytes(s);
    return ws;
}

static std::string wstring_2_string(const std::wstring& ws)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::string s = cvt.to_bytes(ws);
    return s;
}

static void msgBox(const std::string& msg)
{
    ::OutputDebugStringA(msg.c_str());
}



static ID3DBlobPtr compileLibrary(const WCHAR* filename, const WCHAR* targetString)
{
    // Initialize the helper
    gDxcDllHelper.Initialize();
    IDxcCompilerPtr pCompiler;
    IDxcLibraryPtr pLibrary;
    gDxcDllHelper.CreateInstance(CLSID_DxcCompiler, &pCompiler);
    gDxcDllHelper.CreateInstance(CLSID_DxcLibrary, &pLibrary);

    // Open and read the file
    std::ifstream shaderFile(filename);
    if (shaderFile.good() == false)
    {
        msgBox("Can't open file " + wstring_2_string(std::wstring(filename)));
        return nullptr;
    }
    std::stringstream strStream;
    strStream << shaderFile.rdbuf();
    std::string shader = strStream.str();

    // Create blob from the string
    IDxcBlobEncodingPtr pTextBlob;
    pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)shader.c_str(), (uint32_t)shader.size(), 0, &pTextBlob);

    // Compile
    IDxcOperationResultPtr pResult;
    pCompiler->Compile(pTextBlob, filename, L"", targetString, nullptr, 0, nullptr, 0, nullptr, &pResult);

    // Verify the result
    HRESULT resultCode;
    pResult->GetStatus(&resultCode);
    if (FAILED(resultCode))
    {
        IDxcBlobEncodingPtr pError;
        pResult->GetErrorBuffer(&pError);
        std::string log = convertBlobToString(pError.GetInterfacePtr());
        msgBox("Compiler error:\n" + log);
        return nullptr;
    }

    IDxcBlobPtr pBlob;
    pResult->GetResult(&pBlob);
    return pBlob;
}

static ID3D12RootSignaturePtr createRootSignature(ID3D12Device5Ptr pDevice, const D3D12_ROOT_SIGNATURE_DESC& desc)
{
    ID3DBlobPtr pSigBlob;
    ID3DBlobPtr pErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
    if (FAILED(hr))
    {
        std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
        msgBox(msg);
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
            DxilLibrary dxilLib = createDxilLibrary(L"Data/rths.hlsl");
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
            mpEmptyRootSig = root.pRootSig;
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
            uint64_t heap_start = mpSrvUavHeap->GetGPUDescriptorHandleForHeapStart().ptr;
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

GfxContext::~GfxContext()
{
}

ID3D12ResourcePtr GfxContext::createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props)
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

AccelerationStructureBuffers GfxContext::createBottomLevelAS(ID3D12ResourcePtr vb)
{
    D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
    geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geomDesc.Triangles.VertexBuffer.StartAddress = vb->GetGPUVirtualAddress();
    geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(float3);
    geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geomDesc.Triangles.VertexCount = 3;
    geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    // Get the size requirements for the scratch and AS buffers
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = 1;
    inputs.pGeometryDescs = &geomDesc;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
    AccelerationStructureBuffers buffers;
    buffers.scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.result = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

    // Create the bottom-level AS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.DestAccelerationStructureData = buffers.result->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.result->GetGPUVirtualAddress();

    m_cmd_list->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.result;
    m_cmd_list->ResourceBarrier(1, &uavBarrier);

    return buffers;
}

AccelerationStructureBuffers GfxContext::createTopLevelAS(ID3D12ResourcePtr bottom_level_as, uint64_t& tlas_size)
{
    // First, get the size of the TLAS buffers and create them
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
    inputs.NumDescs = 1;
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

    // Create the buffers
    AccelerationStructureBuffers buffers;
    buffers.scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    buffers.result = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);
    tlas_size = info.ResultDataMaxSizeInBytes;

    // The instance desc should be inside a buffer, create and map the buffer
    buffers.instance_desc = createBuffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, kUploadHeapProps);
    D3D12_RAYTRACING_INSTANCE_DESC* instance_desk;
    buffers.instance_desc->Map(0, nullptr, (void**)&instance_desk);

    // Initialize the instance desc. We only have a single instance
    instance_desk->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
    instance_desk->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
    instance_desk->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    float4x4 m; // Identity matrix
    memcpy(instance_desk->Transform, &m, sizeof(instance_desk->Transform));
    instance_desk->AccelerationStructure = bottom_level_as->GetGPUVirtualAddress();
    instance_desk->InstanceMask = 0xFF;

    // Unmap
    buffers.instance_desc->Unmap(0, nullptr);

    // Create the TLAS
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
    asDesc.Inputs = inputs;
    asDesc.Inputs.InstanceDescs = buffers.instance_desc->GetGPUVirtualAddress();
    asDesc.DestAccelerationStructureData = buffers.result->GetGPUVirtualAddress();
    asDesc.ScratchAccelerationStructureData = buffers.result->GetGPUVirtualAddress();

    m_cmd_list->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

    // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
    D3D12_RESOURCE_BARRIER uavBarrier = {};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = buffers.result;
    m_cmd_list->ResourceBarrier(1, &uavBarrier);

    return buffers;
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
    m_as_buffers.scratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
    m_as_buffers.result = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, kDefaultHeapProps);

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
    ret.size = sizeof(float) * 12;
    ret.resource = createBuffer(ret.size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, kDefaultHeapProps);
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

uint64_t GfxContext::submitCommandList()
{
    m_cmd_list->Close();
    ID3D12CommandList* cmd_list = m_cmd_list.GetInterfacePtr();
    m_cmd_queue->ExecuteCommandLists(1, &cmd_list);
    m_fence_value++;
    m_cmd_queue->Signal(m_fence, m_fence_value);
    m_fence->SetEventOnCompletion(m_fence_value, m_fence_event);
    return m_fence_value;
}

void GfxContext::flush()
{
    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_cmd_list->ClearRenderTargetView(m_rtv, clear_color, 0, nullptr);
    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);


    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    D3D12_DISPATCH_RAYS_DESC rt_desc = {};
    rt_desc.Width = m_render_target.width;
    rt_desc.Height = m_render_target.height;
    rt_desc.Depth = 1;

    // todo

    //// RayGen is the first entry in the shader-table
    //rt_desc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress() + 0 * mShaderTableEntrySize;
    //rt_desc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

    //// Miss is the second entry in the shader-table
    //size_t missOffset = 1 * mShaderTableEntrySize;
    //rt_desc.MissShaderTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + missOffset;
    //rt_desc.MissShaderTable.StrideInBytes = mShaderTableEntrySize;
    //rt_desc.MissShaderTable.SizeInBytes = mShaderTableEntrySize;   // Only a s single miss-entry

    //// Hit is the third entry in the shader-table
    //size_t hitOffset = 2 * mShaderTableEntrySize;
    //rt_desc.HitGroupTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + hitOffset;
    //rt_desc.HitGroupTable.StrideInBytes = mShaderTableEntrySize;
    //rt_desc.HitGroupTable.SizeInBytes = mShaderTableEntrySize;


    //// Bind the empty root signature
    //m_cmd_list->SetComputeRootSignature(mpEmptyRootSig);

    //// Dispatch
    //m_cmd_list->SetPipelineState1(mpPipelineState.GetInterfacePtr());
    //m_cmd_list->DispatchRays(&rt_desc);

    // Copy the results to the back-buffer
    addResourceBarrier(m_render_target.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);

    submitCommandList();
}

void GfxContext::finish()
{
    WaitForSingleObject(m_fence_event, INFINITE);
    m_cmd_allocator->Reset();
    m_cmd_list->Reset(m_cmd_allocator, nullptr);

    GetResourceTranslator(m_device)->copyRenderTarget(m_render_target_unity, m_render_target.resource);
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