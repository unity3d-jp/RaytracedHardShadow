#pragma once

#include "rthsTypes.h"

namespace rths {

#define DEF_PTR(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
DEF_PTR(ID3D12Device5);
DEF_PTR(ID3D12GraphicsCommandList4);
DEF_PTR(ID3D12CommandQueue);
DEF_PTR(IDXGISwapChain3);
DEF_PTR(IDXGIFactory4);
DEF_PTR(IDXGIAdapter1);
DEF_PTR(ID3D12Fence);
DEF_PTR(ID3D12CommandAllocator);
DEF_PTR(ID3D12Resource);
DEF_PTR(ID3D12DescriptorHeap);
DEF_PTR(ID3D12Debug);
DEF_PTR(ID3D12StateObject);
DEF_PTR(ID3D12RootSignature);
DEF_PTR(ID3DBlob);
DEF_PTR(IDxcBlobEncoding);
#undef MAKE_SMART_COM_PTR

class GfxContext
{
public:

private:
    IDXGISwapChain3Ptr m_swapchain;
    ID3D12Device5Ptr m_device;
    ID3D12CommandQueuePtr m_cmd_queue;
    ID3D12GraphicsCommandList4Ptr m_cmd_list;
    ID3D12FencePtr m_fence;
};

} // namespace rths
