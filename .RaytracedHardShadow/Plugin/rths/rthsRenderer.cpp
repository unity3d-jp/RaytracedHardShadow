#include "pch.h"
#include "rthsRenderer.h"

namespace rths {

Renderer::Renderer()
{
}

Renderer::~Renderer()
{
}

void Renderer::beginScene()
{
}

void Renderer::endScene()
{
}

void Renderer::render()
{
}

void Renderer::setRenderTarget(void *rt)
{
}

void Renderer::setCamera(const float4x4& trans, float near_, float far_, float fov)
{
}

void Renderer::addDirectionalLight(const float4x4& trans)
{
}

void Renderer::addMesh(const float4x4& trans, void *vb, void *ib)
{
}

} // namespace rths
