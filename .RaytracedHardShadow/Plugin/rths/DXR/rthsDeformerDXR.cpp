#include "pch.h"
#ifdef _WIN32
#include "rthsLog.h"
#include "rthsMisc.h"
#include "rthsGfxContextDXR.h"
#include "rthsDeformerDXR.h"

// shader binaries
#include "rthsDeform.h"

namespace rths {

enum class DeformFlag : int
{
    Blendshape = 1,
    Skinning = 2,
};

struct BlendshapeFrame
{
    int delta_offset;
    float weight;
};
struct BlendshapeInfo
{
    int frame_count;
    int frame_offset;
};

struct BoneCount
{
    int weight_count;
    int weight_offset;
};
struct MeshInfo
{
    int deform_flags;
    int vertex_stride; // in element (e.g. 6 if position + normal)
    int2 pad1;
};



DeformerDXR::DeformerDXR(ID3D12Device5Ptr device)
    : m_device(device)
{
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_cmd_allocator));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmd_allocator, nullptr, IID_PPV_ARGS(&m_cmd_list));
        m_cmd_list->Close();
    }
    {
        {
            D3D12_COMMAND_QUEUE_DESC desc{};
            desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
            desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
            m_device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_cmd_queue_compute));
        }
        m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_cmd_allocator_compute));
        m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_cmd_allocator_compute, nullptr, IID_PPV_ARGS(&m_cmd_list_compute));
        m_cmd_list_compute->Close();
    }

    {
        const D3D12_DESCRIPTOR_RANGE ranges[] = {
                { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 0 },
                { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0, 0, 0 },
                { D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 0 },
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

    TimestampInitialize(m_timestamp, m_device);
}

DeformerDXR::~DeformerDXR()
{
}

bool DeformerDXR::prepare(int render_flags)
{
    m_clamp_blendshape_weights = (render_flags & (int)RenderFlag::ClampBlendShapeWights) != 0;

    bool ret = true;
    if (m_needs_execute_and_reset) {
        m_needs_execute_and_reset = false;

        if (FAILED(m_cmd_allocator->Reset()))
            ret = false;
        else if (FAILED(m_cmd_list->Reset(m_cmd_allocator, nullptr)))
            ret = false;

        if (FAILED(m_cmd_allocator_compute->Reset()))
            ret = false;
        else if (FAILED(m_cmd_list_compute->Reset(m_cmd_allocator_compute, m_pipeline_state)))
            ret = false;
    }

    TimestampReset(m_timestamp);
    TimestampQuery(m_timestamp, m_cmd_list_compute, "DeformerDXR: begin");
    return ret;
}

bool DeformerDXR::deform(MeshInstanceDataDXR& inst_dxr)
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
        desc.NumDescriptors = 32;
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&inst_dxr.srvuav_heap));
    }
    auto handle_allocator = DescriptorHeapAllocatorDXR(m_device, inst_dxr.srvuav_heap);
    auto hdst_vertices = handle_allocator.allocate();
    auto hbase_vertices = handle_allocator.allocate();
    auto hbs_delta = handle_allocator.allocate();
    auto hbs_frames = handle_allocator.allocate();
    auto hbs_info = handle_allocator.allocate();
    auto hbs_weights = handle_allocator.allocate();
    auto hbone_counts = handle_allocator.allocate();
    auto hbone_weights = handle_allocator.allocate();
    auto hbone_matrices = handle_allocator.allocate();
    auto hmesh_info = handle_allocator.allocate();

    if (!inst_dxr.deformed_vertices) {
        // deformed vertices
        inst_dxr.deformed_vertices = createBuffer(sizeof(float4) * vertex_count, kDefaultHeapProps, true);
    }
    addResourceBarrier(inst_dxr.deformed_vertices, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    createUAV(hdst_vertices.hcpu, inst_dxr.deformed_vertices, vertex_count, sizeof(float4));

    // interpret source vertex buffer as just an array of float
    createSRV(hbase_vertices.hcpu, mesh_dxr.vertex_buffer->resource, mesh_dxr.vertex_buffer->size / 4, 4);

    // blendshape
    if (blendshape_count > 0) {
        int frame_count = 0;
        for (auto& bs : mesh.blendshapes) {
            frame_count += (int)bs.frames.size();
        }

        if (!mesh_dxr.bs_delta) {
            // delta
            mesh_dxr.bs_delta = createBuffer(sizeof(float4) * vertex_count * frame_count, kUploadHeapProps);
            writeBuffer(mesh_dxr.bs_delta, [&](void *dst_) {
                auto dst = (float4*)dst_;
                for (auto& bs : mesh.blendshapes) {
                    for (auto& frame : bs.frames) {
                        auto& delta = frame.delta;
                        for (int vi = 0; vi < vertex_count; ++vi)
                            *dst++ = to_float4(delta[vi], 0.0f);
                    }
                }
            });

            // frame
            mesh_dxr.bs_frames = createBuffer(sizeof(BlendshapeFrame) * frame_count, kUploadHeapProps);
            writeBuffer(mesh_dxr.bs_frames, [&](void *dst_) {
                auto dst = (BlendshapeFrame*)dst_;
                int offset = 0;
                for (auto& bs : mesh.blendshapes) {
                    for (auto& frame : bs.frames) {
                        BlendshapeFrame tmp{};
                        tmp.delta_offset = offset;
                        tmp.weight = frame.weight / 100.0f; // 0-100 -> 0.0-1.0
                        *dst++ = tmp;

                        offset += vertex_count;
                    }
                }
            });

            // counts
            mesh_dxr.bs_info = createBuffer(sizeof(BlendshapeInfo) * blendshape_count, kUploadHeapProps);
            writeBuffer(mesh_dxr.bs_info, [&](void *dst_) {
                auto dst = (BlendshapeInfo*)dst_;
                int offset = 0;
                for (auto& bs : mesh.blendshapes) {
                    BlendshapeInfo tmp{};
                    tmp.frame_count = (int)bs.frames.size();
                    tmp.frame_offset = offset;
                    *dst++ = tmp;

                    offset += tmp.frame_count;
                }
            });
        }

        // weights
        {
            if (!inst_dxr.bs_weights) {
                inst_dxr.bs_weights = createBuffer(sizeof(float) * blendshape_count, kUploadHeapProps);
            }
            // update on every frame
            writeBuffer(inst_dxr.bs_weights, [&](void *dst_) {
                auto dst = (float*)dst_;
                for (int bsi = 0; bsi < blendshape_count; ++bsi) {
                    float weight = inst.blendshape_weights[bsi];
                    if (m_clamp_blendshape_weights)
                        weight = clamp(weight, 0.0f, mesh.blendshapes[bsi].frames.back().weight);
                    *dst++ = weight / 100.0f; // 0-100 -> 0.0-1.0;
                }
            });
        }

        createSRV(hbs_delta.hcpu, mesh_dxr.bs_delta, vertex_count * frame_count, sizeof(float4));
        createSRV(hbs_frames.hcpu, mesh_dxr.bs_frames, frame_count, sizeof(BlendshapeFrame));
        createSRV(hbs_info.hcpu, mesh_dxr.bs_info, blendshape_count, sizeof(BlendshapeInfo));
        createSRV(hbs_weights.hcpu, inst_dxr.bs_weights, blendshape_count, sizeof(float));
    }

    // skinning 
    if (bone_count > 0) {
        // bone counts & weights
        if (!mesh_dxr.bone_counts) {
            mesh_dxr.bone_counts = createBuffer(sizeof(BoneCount) * vertex_count, kUploadHeapProps);

            int weight_offset = 0;
            writeBuffer(mesh_dxr.bone_counts, [&](void *dst_) {
                auto dst = (BoneCount*)dst_;
                for (int vi = 0; vi < vertex_count; ++vi) {
                    int n = mesh.skin.bone_counts[vi];
                    *dst++ = { n, weight_offset };
                    weight_offset += n;
                }
            });

            const int weight_count = weight_offset;
            mesh_dxr.bone_weights = createBuffer(sizeof(BoneWeight) * weight_count, kUploadHeapProps);
            writeBuffer(mesh_dxr.bone_weights, [&](void *dst_) {
                auto dst = (BoneWeight*)dst_;
                for (int wi = 0; wi < weight_count; ++wi) {
                    auto& w1 = mesh.skin.weights[wi];
                    *dst++ = { w1.weight, w1.index };
                }
            });
        }

        // bone matrices
        {
            if (!inst_dxr.bone_matrices) {
                inst_dxr.bone_matrices = createBuffer(sizeof(float4x4) * bone_count, kUploadHeapProps);
            }
            // update on every frame
            writeBuffer(inst_dxr.bone_matrices, [&](void *dst_) {
                auto dst = (float4x4*)dst_;

                auto iroot = invert(inst.transform);
                for (int bi = 0; bi < bone_count; ++bi) {
                    *dst++ = mesh.skin.bindposes[bi] * inst.bones[bi] * iroot;
                }
            });
        }

        int weight_count = (int)mesh.skin.weights.size();
        createSRV(hbone_counts.hcpu, mesh_dxr.bone_counts, vertex_count, sizeof(BoneCount));
        createSRV(hbone_weights.hcpu, mesh_dxr.bone_weights, weight_count, sizeof(BoneWeight));
        createSRV(hbone_matrices.hcpu, inst_dxr.bone_matrices, bone_count, sizeof(float4x4));
    }

    // mesh info
    int mesh_info_size = align_to(256, sizeof(MeshInfo));
    if (!mesh_dxr.mesh_info) {
        mesh_dxr.mesh_info = createBuffer(mesh_info_size, kUploadHeapProps);
        writeBuffer(mesh_dxr.mesh_info, [&](void *dst_) {
            MeshInfo info{};
            info.vertex_stride = mesh_dxr.getVertexStride() / 4;
            info.deform_flags = 0;
            if (blendshape_count > 0)
                info.deform_flags |= (int)DeformFlag::Blendshape;
            if (bone_count > 0)
                info.deform_flags |= (int)DeformFlag::Skinning;

            *(MeshInfo*)dst_ = info;
        });
    }
    createCBV(hmesh_info.hcpu, mesh_dxr.mesh_info, mesh_info_size);

    {
        m_cmd_list_compute->SetComputeRootSignature(m_rootsig_deform);

        ID3D12DescriptorHeap* heaps[] = { inst_dxr.srvuav_heap };
        m_cmd_list_compute->SetDescriptorHeaps(_countof(heaps), heaps);
        m_cmd_list_compute->SetComputeRootDescriptorTable(0, hdst_vertices.hgpu);
        m_cmd_list_compute->SetComputeRootDescriptorTable(1, hbase_vertices.hgpu);
        m_cmd_list_compute->SetComputeRootDescriptorTable(2, hmesh_info.hgpu);
        m_cmd_list_compute->Dispatch(mesh.vertex_count, 1, 1);

        m_needs_execute_and_reset = true;
    }

    return true;
}

uint64_t DeformerDXR::execute(ID3D12FencePtr fence, uint64_t fence_value)
{
    TimestampQuery(m_timestamp, m_cmd_list_compute, "DeformerDXR: end");
    TimestampResolve(m_timestamp, m_cmd_list_compute);
    if (!m_needs_execute_and_reset || FAILED(m_cmd_list->Close()) || FAILED(m_cmd_list_compute->Close()))
        return 0;
    {
        ID3D12CommandList* cmd_list[] = { m_cmd_list.GetInterfacePtr() };
        m_cmd_queue->ExecuteCommandLists(_countof(cmd_list), cmd_list);
        m_cmd_queue->Signal(fence, fence_value);
        m_cmd_queue_compute->Wait(fence, fence_value);
    }
    ++fence_value;
    {
        ID3D12CommandList* cmd_list[] = { m_cmd_list_compute.GetInterfacePtr() };
        m_cmd_queue_compute->ExecuteCommandLists(_countof(cmd_list), cmd_list);
        m_cmd_queue_compute->Signal(fence, fence_value);
    }
    return fence_value;
}

void DeformerDXR::onFinish()
{
}

void DeformerDXR::debugPrint()
{
    TimestampPrint(m_timestamp, m_cmd_queue_compute);
}


void DeformerDXR::addResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state_before;
    barrier.Transition.StateAfter = state_after;
    m_cmd_list->ResourceBarrier(1, &barrier);
}

void DeformerDXR::createSRV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int num_elements, int stride)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = num_elements;
    desc.Buffer.StructureByteStride = stride;
    desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    m_device->CreateShaderResourceView(res, &desc, dst);
}

void DeformerDXR::createUAV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int num_elements, int stride)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = 0;
    desc.Buffer.NumElements = num_elements;
    desc.Buffer.StructureByteStride = stride;
    desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    m_device->CreateUnorderedAccessView(res, nullptr, &desc, dst);
}

void DeformerDXR::createCBV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int size)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC desc{};
    desc.BufferLocation = res->GetGPUVirtualAddress();
    desc.SizeInBytes = size;
    m_device->CreateConstantBufferView(&desc, dst);
}


ID3D12ResourcePtr DeformerDXR::createBuffer(int size, const D3D12_HEAP_PROPERTIES& heap_props, bool uav)
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
    desc.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;

    ID3D12ResourcePtr ret;
    auto hr = m_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ret));
    if (FAILED(hr)) {
        SetErrorLog("CreateCommittedResource() failed\n");
    }
    return ret;
}

template<class Body>
bool DeformerDXR::writeBuffer(ID3D12Resource *res, const Body& body)
{
    void *data;
    auto hr = res->Map(0, nullptr, &data);
    if (SUCCEEDED(hr)) {
        body(data);
        res->Unmap(0, nullptr);
        return true;
    }
    else {
        SetErrorLog("Map() failed\n");
    }
    return false;
}


} // namespace rths
#endif // _WIN32
