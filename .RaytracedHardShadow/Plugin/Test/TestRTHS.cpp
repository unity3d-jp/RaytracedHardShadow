#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#include "../rths/rths.h"



TestCase(TestRender)
{
    // create renderer
    auto renderer = rthsRendererCreate();
    if (!renderer) {
        Print("rthsCreateRenderer() retruned null: %s\n", rthsGetErrorLog());
        return;
    }

    std::vector<rths::MeshData*> meshes;
    std::vector<rths::MeshInstanceData*> instances;

    // create mesh
    {

    }

    // create mesh instances
    {

    }

    // render!
    rthsMarkFrameBegin();
    {
        rthsRendererBeginScene(renderer);
        for (auto inst : instances)
            rthsRendererAddGeometry(renderer, inst);

        rthsRendererEndScene(renderer);

        rthsRendererStartRender(renderer);
        rthsRendererFinishRender(renderer);
    }
    rthsMarkFrameEnd();


    // cleanup
    for (auto inst : instances)
        rthsMeshInstanceRelease(inst);
    for (auto mesh : meshes)
        rthsMeshRelease(mesh);
    rthsRendererRelease(renderer);
}

