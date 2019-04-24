using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace UTJ.RaytracedHardShadow
{
    public struct rthsShadowRenderer
    {
        #region internal
        public IntPtr self;
        [DllImport("rths")] static extern byte rthsInitializeGfxDevice();
        [DllImport("rths")] static extern void rthsFinalizeGfxDevice();

        [DllImport("rths")] static extern IntPtr rthsCreateRenderer();
        [DllImport("rths")] static extern void rthsDestroyRenderer(IntPtr self);

        [DllImport("rths")] static extern void rthsSetRenderTarget(IntPtr self, IntPtr rt);
        [DllImport("rths")] static extern void rthsBeginScene(IntPtr self);
        [DllImport("rths")] static extern void rthsEndScene(IntPtr self);
        [DllImport("rths")] static extern void rthsRender(IntPtr self);
        [DllImport("rths")] static extern void rthsSetCamera(IntPtr self, Matrix4x4 trans, float near, float far, float fov);
        [DllImport("rths")] static extern void rthsAddDirectionalLight(IntPtr self, Matrix4x4 trans);
        [DllImport("rths")] static extern void rthsAddMesh(IntPtr self, Matrix4x4 trans, IntPtr vb, IntPtr ib);
        #endregion

        public static implicit operator bool(rthsShadowRenderer v) { return v.self != IntPtr.Zero; }
        public static rthsShadowRenderer Create()
        {
            // return null if not supported
            return new rthsShadowRenderer { self = rthsCreateRenderer() };
        }

        public void Destroy()
        {
            rthsDestroyRenderer(self); self = IntPtr.Zero;
        }

        public void SetRenderTarget(RenderTexture rt)
        {
            rthsSetRenderTarget(self, rt.GetNativeTexturePtr());
        }

        public void BeginScene()
        {
            rthsBeginScene(self);
        }

        public void EndScene()
        {
            rthsEndScene(self);
        }

        public void Render()
        {
            rthsRender(self);
        }

        public void SetCamera(Camera cam)
        {
            rthsSetCamera(self, cam.transform.localToWorldMatrix, cam.nearClipPlane, cam.farClipPlane, cam.fieldOfView);
        }

        public bool AddLight(Light light)
        {
            switch (light.type)
            {
                case LightType.Directional:
                    rthsAddDirectionalLight(self, light.transform.localToWorldMatrix);
                    return true;

                default:
                    Debug.LogWarning("rthsShadowRenderer: " + light.type + " is not supported");
                    return false;
            }
        }

        public void AddMesh(MeshRenderer mr)
        {
            var mf = mr.GetComponent<MeshFilter>();
            var mesh = mf.mesh;
            if (mesh == null)
                return;
            rthsAddMesh(self, mr.transform.localToWorldMatrix, mesh.GetNativeVertexBufferPtr(0), mesh.GetNativeIndexBufferPtr());
        }

        public void AddMesh(SkinnedMeshRenderer smr)
        {
            if (smr.sharedMesh == null)
                return;
            var mesh = new Mesh();
            smr.BakeMesh(mesh);
            rthsAddMesh(self, smr.transform.localToWorldMatrix, mesh.GetNativeVertexBufferPtr(0), mesh.GetNativeIndexBufferPtr());
        }
    }
}
