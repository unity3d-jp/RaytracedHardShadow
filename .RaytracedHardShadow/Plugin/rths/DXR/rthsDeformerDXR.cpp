#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsDeformerDXR.h"

// shader binaries
#include "rthsDeform.h"

#define kNumParams 10

namespace rths {

DeformerDXR::DeformerDXR(ID3D12Device5Ptr device, ID3D12FencePtr fence)
    : m_device(device)
    , m_fence(fence)
{
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_cmd_allocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_cmd_allocator, nullptr, IID_PPV_ARGS(&m_cmd_queue));
    }

    {
        D3D12_DESCRIPTOR_RANGE ranges[kNumParams] = {
            { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 1 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, 2 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, 3 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, 4 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0, 5 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0, 6 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6, 0, 7 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7, 0, 8 },
            { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 9 },
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

        ID3DBlobPtr sig_blob;
        ID3DBlobPtr error_blob;
        HRESULT hr = ::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig_blob, &error_blob);
        if (FAILED(hr)) {
            SetErrorLog(ToString(error_blob) + "\n");
        }
        else {
            hr = m_device->CreateRootSignature(0, sig_blob->GetBufferPointer(), sig_blob->GetBufferSize(), IID_PPV_ARGS(&m_rootsig_deform));
            if (FAILED(hr)) {
                SetErrorLog("CreateRootSignature() failed\n");
            }
        }
    }

    if (m_rootsig_deform) {
        D3D12_COMPUTE_PIPELINE_STATE_DESC psd {};
        psd.pRootSignature = m_rootsig_deform.GetInterfacePtr();
        psd.CS.pShaderBytecode = rthsDeform;
        psd.CS.BytecodeLength = sizeof(rthsDeform);

        HRESULT hr = m_device->CreateComputePipelineState(&psd, IID_PPV_ARGS(&m_pipeline_state));
        if (FAILED(hr)) {
            SetErrorLog("CreateComputePipelineState() failed\n");
        }
    }
}

DeformerDXR::~DeformerDXR()
{
}

bool DeformerDXR::queueDeformCommand(MeshInstanceDataDXR& inst)
{
    if (!m_rootsig_deform || !m_pipeline_state)
        return false;

    auto inst_base = (MeshInstanceData&)inst;
    if (inst_base.bones.num_bones == 0 && inst_base.blendshape_weights.num_blendshapes == 0)
        return false;

    if (!inst.srvuav_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = kNumParams;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&inst.srvuav_heap));
    }

    UINT handle_stride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto hcpu_base = inst.srvuav_heap->GetCPUDescriptorHandleForHeapStart();
    auto hgpu_base = inst.srvuav_heap->GetGPUDescriptorHandleForHeapStart();

    // todo
    return true;
}

} // namespace rths
#endif // _WIN32
