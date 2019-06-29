#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#include "../rths/rths.h"

using rths::float3;


TestCase(TestMinimum)
{
    // create renderer
    auto renderer = rthsRendererCreate();
    if (!renderer) {
        Print("rthsCreateRenderer() retruned null: %s\n", rthsGetErrorLog());
        return;
    }

    std::vector<rths::MeshData*> meshes;
    std::vector<rths::MeshInstanceData*> instances;
    rths::RenderTargetData *render_target = nullptr;

    const int rt_width = 1920;
    const int rt_height = 1080;
    const RenderTargetFormat rt_format = RenderTargetFormat::Ru8;

    // create render target
    {
        render_target = rthsRenderTargetCreate();
        rthsRenderTargetSetup(render_target, rt_width, rt_height, rt_format);
    }

    // create meshes
    {
        // standing triangle
        static const float3 vertices[]{
            {-1.0f, 0.0f, 0.0f},
            { 1.0f, 0.0f, 0.0f},
            { 0.0f, 2.0f, 0.0f},
        };
        static const int indices[]{
            0, 1, 2,
        };
        auto *triangle = rthsMeshCreate();
        rthsMeshSetCPUBuffers(triangle, vertices, indices, sizeof(float3), _countof(vertices), 0, sizeof(int), _countof(indices), 0);
        meshes.push_back(triangle);

        // add a instance with default transform (identity matrix)
        auto instance = rthsMeshInstanceCreate(triangle);
        instances.push_back(instance);
    }
    {
        // floor quad
        static const float3 vertices[]{
            {-5.0f, 0.0f, 5.0f},
            { 5.0f, 0.0f, 5.0f},
            { 5.0f, 0.0f,-5.0f},
            {-5.0f, 0.0f,-5.0f},
        };
        static const int indices[]{
            0, 1, 2, 0, 2, 3,
        };
        auto *quad = rthsMeshCreate();
        meshes.push_back(quad);
        rthsMeshSetCPUBuffers(quad, vertices, indices, sizeof(float3), _countof(vertices), 0, sizeof(int), _countof(indices), 0);

        // add a instance with default transform
        auto instance = rthsMeshInstanceCreate(quad);
        instances.push_back(instance);
    }

    // render!
    rthsMarkFrameBegin();
    rthsRendererBeginScene(renderer);
    rthsRendererSetRenderTarget(renderer, render_target);
    for (auto inst : instances)
        rthsRendererAddGeometry(renderer, inst);

    rthsRendererEndScene(renderer);

    rthsRendererStartRender(renderer);
    rthsRendererFinishRender(renderer);
    rthsMarkFrameEnd();


    // read back render target content
    std::vector<char> rt_buf(rt_width * rt_height);
    if (rthsRendererReadbackRenderTarget(renderer, rt_buf.data())) {
        // todo: export to file
    }


    // cleanup
    for (auto inst : instances)
        rthsMeshInstanceRelease(inst);
    for (auto mesh : meshes)
        rthsMeshRelease(mesh);
    rthsRenderTargetRelease(render_target);
    rthsRendererRelease(renderer);
}

