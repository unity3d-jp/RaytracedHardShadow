#pragma once

namespace rths {

struct float2 { float v[2]; };
struct float3 { float v[3]; };
struct float4 { float v[4]; };
struct float4x4 { float v[16]; };


#define DefPtr(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
DefPtr(ID3D12Device5);
DefPtr(ID3D12GraphicsCommandList4);
DefPtr(ID3D12CommandQueue);
DefPtr(IDXGISwapChain3);
DefPtr(IDXGIFactory4);
DefPtr(IDXGIAdapter1);
DefPtr(ID3D12Fence);
DefPtr(ID3D12CommandAllocator);
DefPtr(ID3D12Resource);
DefPtr(ID3D12DescriptorHeap);
DefPtr(ID3D12Debug);
DefPtr(ID3D12StateObject);
DefPtr(ID3D12RootSignature);
DefPtr(ID3DBlob);
DefPtr(IDxcBlobEncoding);
#undef DefPtr

} // namespace rths
