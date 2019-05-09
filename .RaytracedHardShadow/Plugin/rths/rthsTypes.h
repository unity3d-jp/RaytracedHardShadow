#pragma once

namespace rths {

struct float2 { float v[2]; };
struct float3 { float v[3]; };
struct float4 { float v[4]; };
struct float4x4 { float v[16]; };


#define DefPtr(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
DefPtr(IDXGISwapChain3);
DefPtr(IDXGIFactory4);
DefPtr(IDXGIAdapter1);
DefPtr(IDxcBlobEncoding);
DefPtr(ID3D11Device);
DefPtr(ID3D11DeviceContext);
DefPtr(ID3D11Buffer);
DefPtr(ID3D12Device5);
DefPtr(ID3D12GraphicsCommandList4);
DefPtr(ID3D12CommandQueue);
DefPtr(ID3D12Fence);
DefPtr(ID3D12CommandAllocator);
DefPtr(ID3D12Resource);
DefPtr(ID3D12DescriptorHeap);
DefPtr(ID3D12Debug);
DefPtr(ID3D12StateObject);
DefPtr(ID3D12RootSignature);
DefPtr(ID3D12StateObjectProperties);
DefPtr(ID3DBlob);
DefPtr(IDxcCompiler);
DefPtr(IDxcLibrary);
DefPtr(IDxcBlobEncoding);
DefPtr(IDxcOperationResult);
DefPtr(IDxcBlob);
#undef DefPtr

struct AccelerationStructureBuffers
{
    ID3D12ResourcePtr scratch;
    ID3D12ResourcePtr result;
    ID3D12ResourcePtr instance_desc; // used only for top-level AS
};

struct TextureData
{
    ID3D12ResourcePtr resource;
    int width = 0;
    int height = 0;
};

struct BufferData
{
    ID3D12ResourcePtr resource;
    int size = 0;
};

struct MeshBuffers
{
    BufferData vertex_buffer;
    BufferData index_buffer;
    BufferData transform_buffer;
    int vertex_count = 0;
    int index_count = 0;
    int index_offset = 0;
};

struct LightData
{
    float4 position;
    float4 direction;
    float near_;
    float far_;
    float fov;
    float pad;
};

} // namespace rths
