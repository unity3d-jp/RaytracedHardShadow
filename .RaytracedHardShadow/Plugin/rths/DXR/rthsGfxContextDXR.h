#pragma once
#ifdef _WIN32
#include "rthsRenderer.h"
#include "rthsTypesDXR.h"
#include "rthsResourceTranslatorDXR.h"
#include "rthsDeformerDXR.h"

#define rthsMaxBounce 2

namespace rths {

class GfxContextDXR : public ISceneCallback
{
public:
    static bool initializeInstance();
    static void finalizeInstance();
    static GfxContextDXR* getInstance();

    bool initialize();
    void clear();

    void frameBegin() override;
    void prepare(RenderDataDXR& rd);
    void setSceneData(RenderDataDXR& rd, SceneData& data);
    void setRenderTarget(RenderDataDXR& rd, RenderTargetData *rt);
    void setGeometries(RenderDataDXR& rd, std::vector<GeometryData>& geometries);
    void flush(RenderDataDXR& rd);
    bool finish(RenderDataDXR& rd);
    void frameEnd() override;

    bool readbackRenderTarget(RenderDataDXR& rd, void *dst);
    void clearResourceCache();

    void onMeshDelete(MeshData *mesh) override;
    void onMeshInstanceDelete(MeshInstanceData *inst) override;
    void onRenderTargetDelete(RenderTargetData *rt) override;


    bool valid() const;
    bool checkError();
    ID3D12Device5Ptr getDevice();
    ID3D12CommandQueuePtr getComputeQueue();
    ID3D12FencePtr getFence();

    uint64_t incrementFenceValue();
    void setFenceValue(uint64_t v);

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);

    void addResourceBarrier(ID3D12GraphicsCommandList *cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    uint64_t submitCommandList(ID3D12GraphicsCommandList *cl, uint64_t preceding_fv = 0, bool emit_signal = true);
    uint64_t submitCommandList(const std::vector<ID3D12CommandList*>& cl, uint64_t preceding_fv = 0, bool emit_signal = true);
    uint64_t submitCommandList(ID3D12CommandList *const*cl, size_t n, uint64_t preceding_fv = 0, bool emit_signal = true);

    bool readbackBuffer(RenderDataDXR& rd, void *dst, ID3D12Resource *src, UINT64 size);
    bool uploadBuffer(RenderDataDXR& rd, ID3D12Resource *dst, const void *src, UINT64 size);
    bool readbackTexture(RenderDataDXR& rd, void *dst, ID3D12Resource *src, UINT width, UINT height, DXGI_FORMAT format);
    bool uploadTexture(RenderDataDXR& rd, ID3D12Resource *dst, const void *src, UINT width, UINT height, DXGI_FORMAT format);
    void executeImmediateCopy(RenderDataDXR& rd, ID3D12GraphicsCommandList4Ptr& cl);

private:
    friend std::unique_ptr<GfxContextDXR> std::make_unique<GfxContextDXR>();
    friend struct std::default_delete<GfxContextDXR>;

    GfxContextDXR();
    ~GfxContextDXR();

    IResourceTranslatorPtr m_resource_translator;
    DeformerDXRPtr m_deformer;

    ID3D12Device5Ptr m_device;
    ID3D12CommandQueuePtr m_cmd_queue_direct, m_cmd_queue_compute, m_cmd_queue_immediate_copy;
    ID3D12FencePtr m_fence;
    uint64_t m_fence_value = 0;
    uint64_t m_fv_last_rays = 0;

    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12RootSignaturePtr m_rootsig;

    std::map<const void*, TextureDataDXRPtr> m_texture_records;
    std::map<const void*, BufferDataDXRPtr> m_buffer_records;
    std::map<MeshData*, MeshDataDXRPtr> m_mesh_records;
    std::map<MeshInstanceData*, MeshInstanceDataDXRPtr> m_meshinstance_records;
    std::map<RenderTargetData*, RenderTargetDataDXRPtr> m_rendertarget_records;
};

} // namespace rths
#endif
