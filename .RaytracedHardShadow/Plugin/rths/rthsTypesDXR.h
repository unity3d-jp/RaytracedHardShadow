#pragma once

#include "rthsTypes.h"

#ifdef _WIN32
namespace rths {

#define DefPtr(_a) _COM_SMARTPTR_TYPEDEF(_a, __uuidof(_a))
DefPtr(IDXGISwapChain3);
DefPtr(IDXGIFactory4);
DefPtr(IDXGIAdapter1);
DefPtr(IDXGIResource);
DefPtr(IDxcBlobEncoding);
DefPtr(ID3D11Device);
DefPtr(ID3D11Device1);
DefPtr(ID3D11DeviceContext);
DefPtr(ID3D11Buffer);
DefPtr(ID3D11Texture2D);
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

struct TextureDataDXR
{
    void *texture = nullptr; // unity
    int width = 0;
    int height = 0;

    ID3D12ResourcePtr resource;
    ID3D11Texture2DPtr temporary_d3d11;
    HANDLE handle = nullptr;
};

struct BufferDataDXR
{
    void *buffer = nullptr; // unity
    int size = 0;

    ID3D12ResourcePtr resource;
    ID3D11BufferPtr temporary_d3d11;
    HANDLE handle = nullptr;
};

struct MeshBuffersDXR
{
    BufferDataDXR vertex_buffer;
    BufferDataDXR index_buffer;
    int vertex_count = 0;
    int index_count = 0;
    int index_offset = 0;
    float3x4 transform;

    ID3D12ResourcePtr blas; // bottom level acceleration structure
};


extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;


} // namespace rths
#endif // _WIN32
