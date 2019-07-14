#pragma once
#ifdef _WIN32
#include "rthsRenderer.h"
#include "rthsTypesDXR.h"
#include "rthsResourceTranslatorDXR.h"
#include "rthsDeformerDXR.h"

namespace rths {

class GfxContextDXR : public ISceneCallback
{
public:
    static bool initializeInstance();
    static void finalizeInstance();
    static GfxContextDXR* getInstance();
    static IResourceTranslator* getResourceTranslator();

    bool initialize();
    void clear();
    bool setPowerStableState(bool v);

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

    ID3D12ResourcePtr createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES state, const D3D12_HEAP_PROPERTIES& heap_props);
    ID3D12ResourcePtr createTexture(int width, int height, DXGI_FORMAT format);

    void addResourceBarrier(ID3D12GraphicsCommandList *cl, ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
    uint64_t submitResourceBarrier(ID3D12ResourcePtr resource, D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after, uint64_t preceding_fv = 0);
    uint64_t submitDirectCommandList(ID3D12GraphicsCommandList *cl, uint64_t preceding_fv = 0);
    uint64_t submitComputeCommandList(ID3D12GraphicsCommandList *cl, uint64_t preceding_fv = 0);
    uint64_t submitCommandList(ID3D12CommandQueue *cq, ID3D12GraphicsCommandList *cl, uint64_t preceding_fv = 0);

    uint64_t readbackBuffer(void *dst, ID3D12Resource *src, UINT64 size);
    uint64_t uploadBuffer(ID3D12Resource *dst, const void *src, UINT64 size, bool immediate = true);
    uint64_t copyBuffer(ID3D12Resource *dst, ID3D12Resource *src, UINT64 size, bool immediate = true);
    uint64_t readbackTexture(void *dst, ID3D12Resource *src, UINT width, UINT height, DXGI_FORMAT format);
    uint64_t uploadTexture(ID3D12Resource *dst, const void *src, UINT width, UINT height, DXGI_FORMAT format, bool immediate = true);
    uint64_t copyTexture(ID3D12Resource *dst, ID3D12Resource *src, bool immediate = true, uint64_t preceding_fv = 0);
    uint64_t submitCopy(ID3D12GraphicsCommandList4Ptr& cl, bool immediate, uint64_t preceding_fv = 0);

private:
    friend std::unique_ptr<GfxContextDXR> std::make_unique<GfxContextDXR>();
    friend struct std::default_delete<GfxContextDXR>;

    GfxContextDXR();
    ~GfxContextDXR();

    IResourceTranslatorPtr m_resource_translator;
    DeformerDXRPtr m_deformer;

    ID3D12Device5Ptr m_device;
    bool m_power_stable_state = false;
    ID3D12CommandQueuePtr m_cmd_queue_direct, m_cmd_queue_compute, m_cmd_queue_copy;
    ID3D12FencePtr m_fence;
    uint64_t m_fence_value = 0;
    uint64_t m_fv_last_rays = 0;

    CommandListManagerDXRPtr m_clm_direct, m_clm_copy;
    FenceEventDXR m_event_copy;
    std::vector<ID3D12ResourcePtr> m_tmp_resources;

    ID3D12RootSignaturePtr m_rootsig;
    ID3D12StateObjectPtr m_pipeline_state;
    ID3D12ResourcePtr m_shader_table;
    uint64_t m_shader_record_size = 0;

    std::map<const void*, TextureDataDXRPtr> m_texture_records;
    std::map<const void*, BufferDataDXRPtr> m_buffer_records;
    std::map<MeshData*, MeshDataDXRPtr> m_mesh_records;
    std::map<MeshInstanceData*, MeshInstanceDataDXRPtr> m_meshinstance_records;
    std::map<RenderTargetData*, RenderTargetDataDXRPtr> m_rendertarget_records;
};

} // namespace rths
#endif
