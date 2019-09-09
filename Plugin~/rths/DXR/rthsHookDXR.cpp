#include "pch.h"
#include "rthsHookDXR.h"

namespace rths {

OnBufferUpdateT g_on_buffer_update;
OnBufferReleaseT g_on_buffer_release;
OnTextureReleaseT g_on_texture_release;

void SetOnBufferUpdate(const OnBufferUpdateT& v)
{
    g_on_buffer_update = v;
}
void SetOnBufferRelease(const OnBufferReleaseT& v)
{
    g_on_buffer_release = v;
}
void SetOnTextureRelease(const OnTextureReleaseT& v)
{
    g_on_texture_release = v;
}
void ClearResourceCallbacks()
{
    g_on_buffer_update = {};
    g_on_buffer_release = {};
    g_on_texture_release = {};
}



template<class T>
static inline void ForceWrite(T &dst, const T &src)
{
    DWORD old_flag;
    ::VirtualProtect(&dst, sizeof(T), PAGE_EXECUTE_READWRITE, &old_flag);
    dst = src;
    ::VirtualProtect(&dst, sizeof(T), old_flag, &old_flag);
}


static void(WINAPI *ID3D11DeviceContextUnmap_orig)(ID3D11DeviceContext *self, ID3D11Resource *pResource, UINT Subresource);
static void WINAPI ID3D11DeviceContextUnmap_hook(ID3D11DeviceContext *self, ID3D11Resource *pResource, UINT Subresource)
{
    ID3D11DeviceContextUnmap_orig(self, pResource, Subresource);
    if (g_on_buffer_update)
        g_on_buffer_update(pResource);
}
template<>
bool InstallHook(ID3D11DeviceContext *dst)
{
    if (ID3D11DeviceContextUnmap_orig)
        return false; // already installed

    void **&vtable = ((void***)dst)[0];
    (void*&)ID3D11DeviceContextUnmap_orig = vtable[15];
    ForceWrite(vtable[15], (void*)ID3D11DeviceContextUnmap_hook);
    return true;
}


static UINT(WINAPI *ID3D11BufferRelease_orig)(ID3D11Buffer *self);
static UINT WINAPI ID3D11BufferRelease_hook(ID3D11Buffer *self)
{
    UINT ret = ID3D11BufferRelease_orig(self);
    if (ret == 0 && g_on_buffer_release)
        g_on_buffer_release(self);
    return ret;
}
template<>
bool InstallHook(ID3D11Buffer *dst)
{
    if (ID3D11BufferRelease_orig)
        return false; // already installed

    void **&vtable = ((void***)dst)[0];
    (void*&)ID3D11BufferRelease_orig = vtable[2];
    ForceWrite(vtable[2], (void*)ID3D11BufferRelease_hook);
    return true;
}

static UINT(WINAPI *ID3D11Texture2DRelease_orig)(ID3D11Texture2D *self);
static UINT WINAPI ID3D11Texture2DRelease_hook(ID3D11Texture2D *self)
{
    UINT ret = ID3D11Texture2DRelease_orig(self);
    if (ret == 0 && g_on_texture_release)
        g_on_texture_release(self);
    return ret;
}
template<>
bool InstallHook(ID3D11Texture2D *dst)
{
    if (ID3D11Texture2DRelease_orig)
        return false; // already installed

    void **&vtable = ((void***)dst)[0];
    (void*&)ID3D11Texture2DRelease_orig = vtable[2];
    ForceWrite(vtable[2], (void*)ID3D11Texture2DRelease_hook);
    return true;
}


static UINT(WINAPI *ID3D12ResourceRelease_orig)(ID3D12Resource *self);
static UINT WINAPI ID3D12ResourceRelease_hook(ID3D12Resource *self)
{
    auto desk = self->GetDesc();
    UINT ret = ID3D12ResourceRelease_orig(self);
    if (ret == 0) {
        if(desk.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
            if (g_on_buffer_release)
                g_on_buffer_release(self);
        else if (desk.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            if (g_on_texture_release)
                g_on_texture_release(self);
    }
    return ret;
}

template<>
bool InstallHook(ID3D12Resource *dst)
{
    if (ID3D12ResourceRelease_orig)
        return false; // already installed

    void **&vtable = ((void***)dst)[0];
    (void*&)ID3D12ResourceRelease_orig = vtable[2];
    ForceWrite(vtable[2], (void*)ID3D12ResourceRelease_hook);
    return true;
}

} // namespace rths
