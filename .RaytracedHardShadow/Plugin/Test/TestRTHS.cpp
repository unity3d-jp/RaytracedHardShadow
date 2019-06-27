#include "pch.h"
#include "Test.h"
#include "MeshGenerator.h"
#include "../rths/rths.h"



TestCase(TestRender)
{
    auto renderer = rthsCreateRenderer();
    if (!renderer) {
        Print("rthsCreateRenderer() retruned null: %s\n", rthsGetErrorLog());
        return;
    }

    rthsReleaseRenderer(renderer);
}

