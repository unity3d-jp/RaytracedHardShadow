#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#include "../rths/rths.h"

#define rthsTestImpl
#include "../rths/Foundation/rthsMath.h"


using rths::float3;
using rths::float4x4;


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

    const int rt_width = 256;
    const int rt_height = 256;
    const RenderTargetFormat rt_format = RenderTargetFormat::Rf32;

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
    {
        rthsMarkFrameBegin();
        rthsRendererBeginScene(renderer);
        rthsRendererSetShadowRayOffset(renderer, 0.0001f);
        rthsRendererSetSelfShadowThreshold(renderer, 0.0001f);
        rthsRendererSetRenderTarget(renderer, render_target);
        {
            int flags = 0;
            //flags |= (int)rths::RenderFlag::CullBackFace;
            //flags |= (int)rths::RenderFlag::IgnoreSelfShadow;
            //flags |= (int)rths::RenderFlag::KeepSelfDropShadow;
            flags |= (int)rths::RenderFlag::GPUSkinning;
            flags |= (int)rths::RenderFlag::ClampBlendShapeWights;
            rthsRendererSetRenderFlags(renderer, flags);

        }
        {
            //float3 pos{ 0.0f, 2.5f, -3.5f };
            //auto view = lookat_lh(pos, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
            //auto proj = perspective(60.0f, 1.0f, 0.3f, 100.0f);

            float3 pos{ 0.0f, 2.5f, -3.5f };
            float4x4 view{ {
                {1, 0, 0, 0},
                {0, 0.944813550f, 0.327608585f, 0},
                {0, 0, 0.327608585f, -0.944813550f},
                {0, -1.21540380, -4.12586880, 1},
            } };
            float4x4 proj{ {
                {0.513588428f, 0, 0, 0},
                {0, 1.73205078f, 0, 0},
                {0, 0, -1.00060010f, -1.0f},
                {0, 0, -0.600180030f, 0},
            } };

            rthsRendererSetCamera(renderer, pos, view, proj);
        }
        {
            rthsRendererAddDirectionalLight(renderer, normalize(float3{ -1.0f, -1.0f, 1.0f }));
        }
        for (auto inst : instances)
            rthsRendererAddMesh(renderer, inst);

        rthsRendererEndScene(renderer);

        rthsRendererStartRender(renderer);
        rthsRendererFinishRender(renderer);

        // read back render target content
        std::vector<float> rt_buf(rt_width * rt_height);
        if (rthsRendererReadbackRenderTarget(renderer, rt_buf.data())) {
            // todo: export to file
        }
        rthsMarkFrameEnd();
    }


    // cleanup
    for (auto inst : instances)
        rthsMeshInstanceRelease(inst);
    for (auto mesh : meshes)
        rthsMeshRelease(mesh);
    rthsRenderTargetRelease(render_target);
    rthsRendererRelease(renderer);
}

