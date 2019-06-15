using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;
#if UNITY_2019_1_OR_NEWER
using Unity.Collections;
#endif

namespace UTJ.RaytracedHardShadow
{
    public static class Misc
    {
        public static string CString(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(ptr);
        }

#if UNITY_2019_1_OR_NEWER
        // explicit layout doesn't work with generics...

        [StructLayout(LayoutKind.Explicit)]
        struct NAByte
        {
            [FieldOffset(0)] public NativeArray<byte> nativeArray;
            [FieldOffset(0)] public IntPtr pointer;
        }
        public static IntPtr ForceGetPointer(ref NativeArray<byte> na)
        {
            var union = new NAByte();
            union.nativeArray = na;
            return union.pointer;
        }

        [StructLayout(LayoutKind.Explicit)]
        struct NABoneWeight1
        {
            [FieldOffset(0)] public NativeArray<BoneWeight1> nativeArray;
            [FieldOffset(0)] public IntPtr pointer;
        }
        public static IntPtr ForceGetPointer(ref NativeArray<BoneWeight1> na)
        {
            var union = new NABoneWeight1();
            union.nativeArray = na;
            return union.pointer;
        }
#endif
    }

    public enum rthsRaytraceFlags
    {
        None = 0,
        IgnoreSelfShadow = 1,
        KeepSelfDropShadow = 2,
    }

    public struct rthsMeshData
    {
        public IntPtr vertexBuffer;
        public IntPtr indexBuffer;
        public int vertexStride; // if 0, treated as sizeOfVertexBuffer / vertexCount
        public int vertexCount;
        public int vertexOffset; // in byte
        public int indexStride;
        public int indexCount;
        public int indexOffset; // in byte
    }

    public struct rthsSkinData
    {
        public IntPtr boneCounts;
        public IntPtr weights1;
        public IntPtr weights4;
        public IntPtr matrices;
        public int numBoneCounts;
        public int numWeights1;
        public int numWeights4;
        public int numMatrices;
    }


    public struct rthsRenderer
    {
        #region internal
        public IntPtr self;
        [DllImport("rths")] static extern IntPtr rthsGetErrorLog();
        [DllImport("rths")] static extern IntPtr rthsCreateRenderer();
        [DllImport("rths")] static extern void rthsDestroyRenderer(IntPtr self);

        [DllImport("rths")] static extern void rthsBeginScene(IntPtr self);
        [DllImport("rths")] static extern void rthsEndScene(IntPtr self);
        [DllImport("rths")] static extern void rthsSetRaytraceFlags(IntPtr self, int flags);
        [DllImport("rths")] static extern void rthsSetShadowRayOffset(IntPtr self, float v);
        [DllImport("rths")] static extern void rthsSetSelfShadowThreshold(IntPtr self, float v);
        [DllImport("rths")] static extern void rthsSetRenderTarget(IntPtr self, IntPtr rt);
        [DllImport("rths")] static extern void rthsSetCamera(IntPtr self, Matrix4x4 trans, Matrix4x4 view, Matrix4x4 proj, float near, float far, float fov);
        [DllImport("rths")] static extern void rthsAddDirectionalLight(IntPtr self, Matrix4x4 trans);
        [DllImport("rths")] static extern void rthsAddSpotLight(IntPtr self, Matrix4x4 trans, float range, float spotAngle);
        [DllImport("rths")] static extern void rthsAddPointLight(IntPtr self, Matrix4x4 trans, float range);
        [DllImport("rths")] static extern void rthsAddReversePointLight(IntPtr self, Matrix4x4 trans, float range);
        [DllImport("rths")] static extern void rthsAddMesh(IntPtr self, rthsMeshData mes, Matrix4x4 trans);
        [DllImport("rths")] static extern void rthsAddSkinnedMesh(IntPtr self, rthsMeshData mes, rthsSkinData skin);

        [DllImport("rths")] static extern IntPtr rthsGetRenderAll();
        #endregion

        public static string errorLog
        {
            get { return Misc.CString(rthsGetErrorLog()); }
        }

        public static implicit operator bool(rthsRenderer v) { return v.self != IntPtr.Zero; }
        public static rthsRenderer Create()
        {
            // rthsCreateRenderer() will return null if DXR is not supported
            return new rthsRenderer { self = rthsCreateRenderer() };
        }

        public void Destroy()
        {
            rthsDestroyRenderer(self);
            self = IntPtr.Zero;
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

        public void SetRaytraceFlags(int flags)
        {
            rthsSetRaytraceFlags(self, flags);
        }
        public void SetShadowRayOffset(float v)
        {
            rthsSetShadowRayOffset(self, v);
        }
        public void SetSelfShadowThreshold(float v)
        {
            rthsSetSelfShadowThreshold(self, v);
        }

        public void SetCamera(Camera cam)
        {
            rthsSetCamera(self, cam.transform.localToWorldMatrix, cam.worldToCameraMatrix, cam.projectionMatrix, cam.nearClipPlane, cam.farClipPlane, cam.fieldOfView);
        }

        public bool AddLight(Light light)
        {
            switch (light.type)
            {
                case LightType.Directional:
                    rthsAddDirectionalLight(self, light.transform.localToWorldMatrix);
                    return true;
                case LightType.Spot:
                    rthsAddSpotLight(self, light.transform.localToWorldMatrix, light.range, light.spotAngle);
                    return true;
                case LightType.Point:
                    rthsAddPointLight(self, light.transform.localToWorldMatrix, light.range);
                    return true;
                default:
                    Debug.LogWarning("rthsShadowRenderer: " + light.type + " is not supported");
                    return false;
            }
        }
        public bool AddLight(ShadowCasterLight light)
        {
            switch (light.lightType)
            {
                case ShadowCasterLightType.Directional:
                    rthsAddDirectionalLight(self, light.transform.localToWorldMatrix);
                    return true;
                case ShadowCasterLightType.Spot:
                    rthsAddSpotLight(self, light.transform.localToWorldMatrix, light.range, light.spotAngle);
                    return true;
                case ShadowCasterLightType.Point:
                    rthsAddPointLight(self, light.transform.localToWorldMatrix, light.range);
                    return true;
                case ShadowCasterLightType.ReversePoint:
                    rthsAddReversePointLight(self, light.transform.localToWorldMatrix, light.range);
                    return true;
                default:
                    Debug.LogWarning("rthsShadowRenderer: " + light.lightType + " is not supported");
                    return false;
            }
        }

        public void AddMesh(Mesh mesh, Matrix4x4 trans, bool isDynamic_)
        {
            int indexStride = mesh.indexFormat == UnityEngine.Rendering.IndexFormat.UInt16 ? 2 : 4;
            byte isDynamic = (byte)(isDynamic_ ? 1 : 0);
            int numSubmeshes = mesh.subMeshCount;
            for (int smi = 0; smi < numSubmeshes; ++smi)
            {
                if (mesh.GetTopology(smi) == MeshTopology.Triangles)
                {
                    var data = default(rthsMeshData);
                    data.vertexBuffer = mesh.GetNativeVertexBufferPtr(0);
                    data.vertexCount = mesh.vertexCount;
                    data.indexBuffer = mesh.GetNativeIndexBufferPtr();
                    data.indexStride = indexStride;
                    data.indexCount = (int)mesh.GetIndexCount(smi);
                    data.indexOffset = (int)mesh.GetIndexStart(smi) * indexStride;
                    rthsAddMesh(self, data, trans);
                }
            }
        }

        public static void IssueRender()
        {
            GL.IssuePluginEvent(rthsGetRenderAll(), 0);
        }
        public static void IssueRender(CommandBuffer cb)
        {
            cb.IssuePluginEvent(rthsGetRenderAll(), 0);
        }
    }
}
