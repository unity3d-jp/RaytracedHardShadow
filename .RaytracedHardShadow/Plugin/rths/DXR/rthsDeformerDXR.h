#pragma once
#ifdef _WIN32
namespace rths {

class DeformerDXR
{
public:
    DeformerDXR(ID3D12Device5Ptr device);
    ~DeformerDXR();

    bool prepare();
    bool queueDeformCommand(MeshInstanceDataDXR& inst);
    bool executeDeform(ID3D12FencePtr fence, UINT64 fence_value);

private:
    void createSRV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int num_elements, int stride);
    void createUAV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int num_elements, int stride);
    void createCBV(D3D12_CPU_DESCRIPTOR_HANDLE dst, ID3D12Resource *res, int size);
    ID3D12ResourcePtr createBuffer(int size, const D3D12_HEAP_PROPERTIES& heap_props, bool uav = false);
    template<class Body> bool writeBuffer(ID3D12Resource *res, const Body& body);


private:
    ID3D12Device5Ptr m_device;

    ID3D12CommandAllocatorPtr m_cmd_allocator;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12CommandQueuePtr m_cmd_queue;

    ID3D12RootSignaturePtr m_rootsig_deform;
    ID3D12PipelineStatePtr m_pipeline_state;
};
using DeformerDXRPtr = std::shared_ptr<DeformerDXR>;

} // namespace rths
#endif // _WIN32
