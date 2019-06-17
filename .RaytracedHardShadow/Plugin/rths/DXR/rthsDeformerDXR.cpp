#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsDeformerDXR.h"

// shader binaries
#include "rthsDeform.h"

namespace rths {

enum DeformFlag
{
    DF_APPLY_BLENDSHAPE = 1,
    DF_APPLY_SKINNING = 2,
};

struct BoneCount
{
    int weight_count;
    int weight_offset;
};

struct MeshInfo
{
    int vertex_count;
    int vertex_stride; // in element (e.g. 6 if position + normals)
    int deform_flags;
    int blendshape_count;
};


static const D3D12_DESCRIPTOR_RANGE g_descriptor_ranges[] = {
        { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 1 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, 2 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0, 3 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3, 0, 4 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4, 0, 5 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5, 0, 6 },
        { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 7 },
};

DeformerDXR::DeformerDXR(ID3D12Device5Ptr device)
    : m_device(device)
{
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_cmd_allocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_cmd_allocator, nullptr, IID_PPV_ARGS(&m_cmd_list));
    }

    {
        D3D12_ROOT_PARAMETER params[_countof(g_descriptor_ranges)]{};
        for (int i = 0; i < _countof(g_descriptor_ranges); i++) {
            auto& param = params[i];
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.DescriptorTable.NumDescriptorRanges = 1;
            param.DescriptorTable.pDescriptorRanges = &g_descriptor_ranges[i];
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

bool DeformerDXR::prepare()
{
    if (SUCCEEDED(m_cmd_allocator->Reset())) {
        if (SUCCEEDED(m_cmd_list->Reset(m_cmd_allocator, nullptr))) {
            return true;
        }
    }
    return false;
}

bool DeformerDXR::queueDeformCommand(MeshInstanceDataDXR& inst_dxr)
{
    if (!m_rootsig_deform || !m_pipeline_state || !inst_dxr.mesh)
        return false;

    auto& inst = *inst_dxr.base;
    auto& mesh = *inst_dxr.mesh->base;
    auto& mesh_dxr = *inst_dxr.mesh;

    int vertex_count = mesh.vertex_count;
    int blendshape_count = (int)inst.blendshape_weights.size();
    int bone_count = (int)inst.bones.size();

    if (blendshape_count == 0 && bone_count == 0)
        return false; // no need to deform

    // setup descriptors
    if (!inst_dxr.srvuav_heap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.NumDescriptors = _countof(g_descriptor_ranges);
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&inst_dxr.srvuav_heap));
    }

    UINT handle_stride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto hcpu_base = inst_dxr.srvuav_heap->GetCPUDescriptorHandleForHeapStart();
    auto hgpu_base = inst_dxr.srvuav_heap->GetGPUDescriptorHandleForHeapStart();

    auto allocate_handle = [this, &hcpu_base, &hgpu_base, handle_stride]() {
        DescriptorHandleDXR ret;
        ret.hcpu = hcpu_base;
        ret.hgpu = hgpu_base;
        hcpu_base.ptr += handle_stride;
        hgpu_base.ptr += handle_stride;
        return ret;
    };

    auto hdst_vertices = allocate_handle();
    auto hbase_vertices = allocate_handle();
    auto hbs_point_delta = allocate_handle();
    auto hbs_point_weights = allocate_handle();
    auto hbone_counts = allocate_handle();
    auto hbone_weights = allocate_handle();
    auto hbone_matrices = allocate_handle();
    auto hmesh_info = allocate_handle();

    auto create_uav = [this](D3D12_CPU_DESCRIPTOR_HANDLE hcpu, ID3D12Resource *res, int num_elements, int stride) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = num_elements;
        desc.Buffer.StructureByteStride = stride;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        m_device->CreateUnorderedAccessView(res, nullptr, &desc, hcpu);
    };

    auto create_srv = [this](D3D12_CPU_DESCRIPTOR_HANDLE hcpu, ID3D12Resource *res, int num_elements, int stride) {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.NumElements = num_elements;
        desc.Buffer.StructureByteStride = stride;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        m_device->CreateShaderResourceView(res, &desc, hcpu);
    };

    auto create_buffer = [this](int size, const D3D12_HEAP_PROPERTIES& heap_props) {
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
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        ID3D12ResourcePtr ret;
        auto hr = m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&ret));
        if (FAILED(hr)) {
            SetErrorLog("CreateCommittedResource() failed\n");
        }
        return ret;
    };

    auto write_buffer = [this](ID3D12Resource *res, const auto& body) {
        void *data;
        auto hr = res->Map(0, nullptr, &data);
        if (SUCCEEDED(hr)) {
            body(data);
            res->Unmap(0, nullptr);
        }
        else {
            SetErrorLog("Map() failed\n");
        }
    };

    if (!inst_dxr.deformed_vertices) {
        // deformed vertices
        inst_dxr.deformed_vertices = create_buffer(sizeof(float4) * vertex_count, kDefaultHeapProps);
        create_uav(hdst_vertices.hcpu, inst_dxr.deformed_vertices, vertex_count, sizeof(float4));

        // base vertices
        // todo: handle mesh.vertex_offset
        create_srv(hbase_vertices.hcpu, mesh_dxr.vertex_buffer->resource, vertex_count, mesh_dxr.getVertexStride());
    }

    // blendshape
    if (blendshape_count > 0) {
        if (!mesh_dxr.bs_point_delta) {
            // point delta
            mesh_dxr.bs_point_delta = create_buffer(sizeof(float4) * vertex_count * blendshape_count, kUploadHeapProps);
            create_srv(hbs_point_delta.hcpu, mesh_dxr.bs_point_delta, vertex_count * blendshape_count, sizeof(float4));
            write_buffer(mesh_dxr.bs_point_delta, [&](void *dst_) {
                auto dst = (float4*)dst_;
                for (int bsi = 0; bsi < blendshape_count; ++bsi) {
                    auto& delta = mesh.blendshapes[bsi].frames[0].delta;
                    for (int vi = 0; vi < vertex_count; ++vi)
                        *dst++ = to_float4(delta[vi], 0.0f);
                }
            });
        }

        // weights
        {
            if (!inst_dxr.blendshape_weights) {
                inst_dxr.blendshape_weights = create_buffer(sizeof(float) * blendshape_count, kUploadHeapProps);
                create_srv(hbs_point_weights.hcpu, inst_dxr.blendshape_weights, blendshape_count, sizeof(float));
            }
            // update on every frame
            write_buffer(inst_dxr.blendshape_weights, [&](void *dst_) {
                std::copy(inst.blendshape_weights.data(), inst.blendshape_weights.data() + inst.blendshape_weights.size(),
                    (float*)dst_);
                });
        }
    }

    // skinning 
    if (bone_count > 0) {
        // bone counts & weights
        if (!mesh_dxr.bone_counts) {
            mesh_dxr.bone_counts = create_buffer(sizeof(BoneCount) * vertex_count, kUploadHeapProps);
            create_srv(hbone_counts.hcpu, mesh_dxr.bone_counts, vertex_count, sizeof(BoneCount));

            int weight_count = 0;
            write_buffer(mesh_dxr.bone_counts, [&](void *dst_) {
                auto dst = (BoneCount*)dst_;
                for (int vi = 0; vi < vertex_count; ++vi) {
                    int n = mesh.skin.bone_counts[vi];
                    *dst++ = { n, weight_count };
                    weight_count += n;
                }
            });

            mesh_dxr.bone_weights = create_buffer(sizeof(BoneWeight) * weight_count, kUploadHeapProps);
            create_srv(hbone_weights.hcpu, mesh_dxr.bone_weights, weight_count, sizeof(BoneWeight));
            write_buffer(mesh_dxr.bone_weights, [&](void *dst_) {
                auto dst = (BoneWeight*)dst_;
                for (int wi = 0; wi < weight_count; ++wi) {
                    auto& w1 = mesh.skin.weights[wi];
                    *dst++ = { w1.weight, w1.index };
                }
            });
        }

        // bone matrices
        {
            if (!inst_dxr.bones) {
                inst_dxr.bones = create_buffer(sizeof(float4x4) * bone_count, kUploadHeapProps);
                create_srv(hbone_matrices.hcpu, inst_dxr.bones, bone_count, sizeof(float4x4));
            }
            // update on every frame
            write_buffer(inst_dxr.bones, [&](void *dst_) {
                auto dst = (float4x4*)dst_;

                auto iroot = invert(inst.transform);
                for (int bi = 0; bi < bone_count; ++bi) {
                    *dst++ = mesh.skin.bindposes[bi] * inst.bones[bi] * iroot;
                }
            });
        }
    }

    // mesh info
    if (!mesh_dxr.mesh_info) {
        mesh_dxr.mesh_info = create_buffer(sizeof(MeshInfo), kUploadHeapProps);
        create_srv(hmesh_info.hcpu, mesh_dxr.mesh_info, 1, sizeof(MeshInfo));
        write_buffer(mesh_dxr.mesh_info, [&](void *dst_) {
            MeshInfo info{};
            info.vertex_count = vertex_count;
            info.vertex_stride = mesh_dxr.getVertexStride() / 4;
            info.deform_flags = 0;
            if (blendshape_count > 0)
                info.deform_flags |= DF_APPLY_BLENDSHAPE;
            if (bone_count > 0)
                info.deform_flags |= DF_APPLY_SKINNING;
            info.blendshape_count = blendshape_count;

            *(MeshInfo*)dst_ = info;
        });
    }

    {
        m_cmd_list->SetComputeRootSignature(m_rootsig_deform);

        ID3D12DescriptorHeap* heaps[] = { inst_dxr.srvuav_heap };
        m_cmd_list->SetDescriptorHeaps(_countof(heaps), heaps);
        m_cmd_list->SetComputeRootDescriptorTable(0, hdst_vertices.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(1, hbase_vertices.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(2, hbs_point_delta.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(3, hbs_point_weights.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(4, hbone_counts.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(5, hbone_weights.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(6, hbone_matrices.hgpu);
        m_cmd_list->SetComputeRootDescriptorTable(7, hmesh_info.hgpu);

        m_cmd_list->Dispatch(mesh.vertex_count, 1, 1);
    }

    return true;
}

bool DeformerDXR::executeCommand(ID3D12FencePtr fence, UINT64 fence_value)
{
    if(FAILED(m_cmd_list->Close()))
        return false;

    ID3D12CommandList* cmd_list[] = { m_cmd_list.GetInterfacePtr() };
    m_cmd_queue->ExecuteCommandLists(_countof(cmd_list), cmd_list);
    m_cmd_queue->Signal(fence, fence_value);
    return true;
}

} // namespace rths
#endif // _WIN32
