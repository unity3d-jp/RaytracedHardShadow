using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;
#if UNITY_2019_1_OR_NEWER
using Unity.Collections;
using UnityEngine.Experimental.Rendering;
#endif

namespace UTJ.RaytracedHardShadow
{
// Object.Equals(object o) & Object.GetHashCode()
#pragma warning disable CS0660, CS0661

    public static class Misc
    {
        public static string CString(IntPtr ptr)
        {
            return ptr == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(ptr);
        }

        public static void SafeDispose<T>(ref T obj) where T : class, IDisposable
        {
            if (obj != null)
            {
                obj.Dispose();
                obj = null;
            }
        }

#if UNITY_2019_1_OR_NEWER
        public static void SafeDispose<T>(ref NativeArray<T> obj) where T : struct
        {
            if (obj.IsCreated)
                obj.Dispose();
        }

        // explicit layout doesn't work with generics...

        [StructLayout(LayoutKind.Explicit)]
        struct NAByte
        {
            [FieldOffset(0)] public NativeArray<byte> nativeArray;
            [FieldOffset(0)] public IntPtr pointer;
        }
        public static IntPtr GetPointer(ref NativeArray<byte> na)
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
        public static IntPtr GetPointer(ref NativeArray<BoneWeight1> na)
        {
            var union = new NABoneWeight1();
            union.nativeArray = na;
            return union.pointer;
        }
#endif
    }

    [Flags]
    public enum rthsDebugFlag : uint
    {
        Timestamp       = 0x01,
        ForceUpdateAS   = 0x02,
        PowerStableState= 0x04,
    }

    [Flags]
    public enum rthsGlobalFlag : uint
    {
        DeferredInitialization = 0x01,
    };

    [Flags]
    public enum rthsRenderFlag : uint
    {
        CullBackFaces           = 0x00000001,
        FlipCasterFaces         = 0x00000002,
        IgnoreSelfShadow        = 0x00000004,
        KeepSelfDropShadow      = 0x00000008,
        AlphaTest               = 0x00000010,
        Transparent             = 0x00000020,
        AdaptiveSampling        = 0x00000100,
        Antialiasing            = 0x00000200,
        GPUSkinning             = 0x00010000,
        ClampBlendShapeWights   = 0x00020000,
    }

    [Flags]
    public enum rthsInstanceFlag : uint
    {
        ReceiveShadows  = 0x01,
        ShadowsOnly     = 0x02,
        CastShadows     = 0x04,
        CullFront       = 0x10,
        CullBack        = 0x20,
        CullFrontShadow = 0x40,
        CullBackShadow  = 0x80,
    };

    public enum rthsRenderTargetFormat : uint
    {
        Unknown = 0,
        Ru8,
        RGu8,
        RGBAu8,
        Rf16,
        RGf16,
        RGBAf16,
        Rf32,
        RGf32,
        RGBAf32,
    }

    public enum rthsOutputFormat : uint
    {
        BitMask = 0,
        Float = 1,
    };


    public struct rthsGlobals
    {
        #region internal
        [DllImport("rths")] static extern IntPtr rthsGetVersion();
        [DllImport("rths")] static extern IntPtr rthsGetReleaseDate();
        [DllImport("rths")] static extern IntPtr rthsGetErrorLog();
        [DllImport("rths")] static extern void rthsClearErrorLog();
        [DllImport("rths")] static extern rthsDebugFlag rthsGlobalsGetDebugFlags();
        [DllImport("rths")] static extern void rthsGlobalsSetDebugFlags(rthsDebugFlag v);
        [DllImport("rths")] static extern rthsGlobalFlag rthsGlobalsGetFlags();
        [DllImport("rths")] static extern void rthsGlobalsSetFlags(rthsGlobalFlag v);
        #endregion

        public static string version
        {
            get { return Misc.CString(rthsGetVersion()); }
        }
        public static string releaseDate
        {
            get { return Misc.CString(rthsGetReleaseDate()); }
        }
        public static string errorLog
        {
            get { return Misc.CString(rthsGetErrorLog()); }
        }
        public static rthsDebugFlag debugFlags
        {
            get { return rthsGlobalsGetDebugFlags(); }
            set { rthsGlobalsSetDebugFlags(value); }
        }
        public static rthsGlobalFlag flags
        {
            get { return rthsGlobalsGetFlags(); }
            set { rthsGlobalsSetFlags(value); }
        }

        public static void ClearErrorLog() { rthsClearErrorLog(); }
    }

    public struct rthsMeshData
    {
        #region internal
        public IntPtr self;
        [DllImport("rths")] static extern IntPtr rthsMeshCreate();
        [DllImport("rths")] static extern void rthsMeshRelease(IntPtr self);
        [DllImport("rths")] static extern byte rthsMeshIsRelocated(IntPtr self);
        [DllImport("rths")] static extern void rthsMeshSetName(IntPtr self, string name);
        [DllImport("rths")] static extern void rthsMeshSetCPUBuffers(IntPtr self, IntPtr vb, IntPtr ib, int vertexStride, int vertexCount, int vertexOffset, int indexStride, int indexCount, int indexOffset);
        [DllImport("rths")] static extern void rthsMeshSetGPUBuffers(IntPtr self, IntPtr vb, IntPtr ib, int vertexStride, int vertexCount, int vertexOffset, int indexStride, int indexCount, int indexOffset);
        [DllImport("rths")] static extern void rthsMeshSetSkinBindposes(IntPtr self, Matrix4x4[] bindposes, int num_bindposes);
        [DllImport("rths")] static extern void rthsMeshSetSkinWeights(IntPtr self, IntPtr c, int nc, IntPtr w, int nw);
        [DllImport("rths")] static extern void rthsMeshSetSkinWeights4(IntPtr self, BoneWeight[] w4, int nw4);
        [DllImport("rths")] static extern void rthsMeshSetBlendshapeCount(IntPtr self, int num_bs);
        [DllImport("rths")] static extern void rthsMeshAddBlendshapeFrame(IntPtr self, int bs_index, Vector3[] delta, float weight);
        [DllImport("rths")] static extern void rthsMeshMarkDyncmic(IntPtr self, byte v);
        #endregion

        public static implicit operator bool(rthsMeshData v) { return v.self != IntPtr.Zero; }
        public static bool operator ==(rthsMeshData a, rthsMeshData b) { return a.self == b.self; }
        public static bool operator !=(rthsMeshData a, rthsMeshData b) { return a.self != b.self; }

        public string name
        {
            set { rthsMeshSetName(self, value); }
        }
        public bool isRelocated
        {
            get { return rthsMeshIsRelocated(self) != 0; }
        }

        public static rthsMeshData Create()
        {
            return new rthsMeshData { self = rthsMeshCreate() };
        }

        public void Release()
        {
            rthsMeshRelease(self);
            self = IntPtr.Zero;
            rthsRenderer.IssueFlushDeferredCommands();
        }

        public void SetGPUBuffers(IntPtr vb, IntPtr ib, int vertexStride, int vertexCount, int vertexOffset, int indexStride, int indexCount, int indexOffset)
        {
            rthsMeshSetGPUBuffers(self, vb, ib, vertexStride, vertexCount, vertexOffset, indexStride, indexCount, indexOffset);
        }

        public void SetBindpose(Matrix4x4[] bindposes)
        {
            rthsMeshSetSkinBindposes(self, bindposes, bindposes.Length);
        }
#if UNITY_2019_1_OR_NEWER
        public void SetSkinWeights(NativeArray<byte> counts, NativeArray<BoneWeight1> weights)
        {
            rthsMeshSetSkinWeights(self, Misc.GetPointer(ref counts), counts.Length, Misc.GetPointer(ref weights), weights.Length);
        }
#endif
        public void SetSkinWeights(BoneWeight[] w4)
        {
            rthsMeshSetSkinWeights4(self, w4, w4.Length);
        }

        public void SetBlendshapeCount(int v)
        {
            rthsMeshSetBlendshapeCount(self, v);
        }

        public void AddBlendshapeFrame(int index, Vector3[] delta, float weight)
        {
            rthsMeshAddBlendshapeFrame(self, index, delta, weight);
        }


        public void SetGPUBuffers(Mesh mesh)
        {
            int indexStride = mesh.indexFormat == UnityEngine.Rendering.IndexFormat.UInt16 ? 2 : 4;
            int indexCountMerged = 0;
            int prevIndexEnd = 0;

            // merge continuous triangle submeshes into a single one
            int subMeshCount = mesh.subMeshCount;
            for (int smi = 0; smi < subMeshCount; ++smi)
            {
                if (mesh.GetTopology(smi) != MeshTopology.Triangles)
                    break;
                int start = (int)mesh.GetIndexStart(smi);
                if (start != prevIndexEnd)
                    break;
                int indexCount = (int)mesh.GetIndexCount(smi);
                indexCountMerged += (int)mesh.GetIndexCount(smi);
                prevIndexEnd = start + indexCount;
            }

            SetGPUBuffers(
                mesh.GetNativeVertexBufferPtr(0), mesh.GetNativeIndexBufferPtr(),
                0, mesh.vertexCount, 0, indexStride, indexCountMerged, 0);
        }

        public void MarkDynamic()
        {
            rthsMeshMarkDyncmic(self, 1);
        }
    }

    public struct rthsMeshInstanceData
    {
        #region internal
        public IntPtr self;
        [DllImport("rths")] static extern IntPtr rthsMeshInstanceCreate(rthsMeshData mesh);
        [DllImport("rths")] static extern void rthsMeshInstanceRelease(IntPtr self);
        [DllImport("rths")] static extern void rthsMeshInstanceSetName(IntPtr self, string name);
        [DllImport("rths")] static extern void rthsMeshInstanceSetFlags(IntPtr self, rthsInstanceFlag flags);
        [DllImport("rths")] static extern void rthsMeshInstanceSetLayer(IntPtr self, int layer);
        [DllImport("rths")] static extern void rthsMeshInstanceSetTransform(IntPtr self, Matrix4x4 transform);
        [DllImport("rths")] static extern void rthsMeshInstanceSetBones(IntPtr self, Matrix4x4[] bones, int num_bones);
        [DllImport("rths")] static extern void rthsMeshInstanceSetBlendshapeWeights(IntPtr self, float[] bsw, int num_bsw);
        #endregion

        public static implicit operator bool(rthsMeshInstanceData v) { return v.self != IntPtr.Zero; }
        public static bool operator ==(rthsMeshInstanceData a, rthsMeshInstanceData b) { return a.self == b.self; }
        public static bool operator !=(rthsMeshInstanceData a, rthsMeshInstanceData b) { return a.self != b.self; }


        public string name
        {
            set { rthsMeshInstanceSetName(self, value); }
        }
        public rthsInstanceFlag flags
        {
            set { rthsMeshInstanceSetFlags(self, value); }
        }
        public int layer
        {
            set { rthsMeshInstanceSetLayer(self, value); }
        }
        public Matrix4x4 transform
        {
            set { rthsMeshInstanceSetTransform(self, value); }
        }

        public static rthsMeshInstanceData Create(rthsMeshData mesh)
        {
            return new rthsMeshInstanceData { self = rthsMeshInstanceCreate(mesh) };
        }

        public void Release()
        {
            rthsMeshInstanceRelease(self);
            self = IntPtr.Zero;
            rthsRenderer.IssueFlushDeferredCommands();
        }

        public void SetBones(Matrix4x4[] bones)
        {
            if (bones == null || bones.Length == 0)
                rthsMeshInstanceSetBones(self, null, 0);
            else
                rthsMeshInstanceSetBones(self, bones, bones.Length);
        }
        public void SetBones(Transform[] bones)
        {
            if (bones == null || bones.Length == 0)
            {
                rthsMeshInstanceSetBones(self, null, 0);
            }
            else
            {
                int n = bones.Length;
                var transforms = new Matrix4x4[n];
                for (int bi = 0; bi < n; ++bi)
                {
                    var bone = bones[bi];
                    transforms[bi] = bone ? bone.localToWorldMatrix : Matrix4x4.identity;
                }
                SetBones(transforms);
            }
        }

        public void SetBlendshapeWeights(float[] bsw)
        {
            if (bsw == null)
                rthsMeshInstanceSetBlendshapeWeights(self, null, 0);
            else
                rthsMeshInstanceSetBlendshapeWeights(self, bsw, bsw.Length);
        }
        public void SetBlendshapeWeights(SkinnedMeshRenderer smr)
        {
            Mesh mesh = null;
            if (smr != null)
                mesh = smr.sharedMesh;

            if (mesh != null)
            {
                int nbs = mesh.blendShapeCount;
                if (nbs > 0)
                {
                    var weights = new float[nbs];
                    for (int bsi = 0; bsi < nbs; ++bsi)
                        weights[bsi] = smr.GetBlendShapeWeight(bsi);
                    SetBlendshapeWeights(weights);
                }
            }
            else
            {
                rthsMeshInstanceSetBlendshapeWeights(self, null, 0);
            }
        }
    }

    public struct rthsRenderTarget
    {
        #region internal
        public IntPtr self;
        [DllImport("rths")] static extern IntPtr rthsRenderTargetCreate();
        [DllImport("rths")] static extern void rthsRenderTargetRelease(IntPtr self);
        [DllImport("rths")] static extern byte rthsRenderTargetIsRelocated(IntPtr self);
        [DllImport("rths")] static extern void rthsRenderTargetSetName(IntPtr self, string name);
        [DllImport("rths")] static extern void rthsRenderTargetSetGPUTexture(IntPtr self, IntPtr tex);
        [DllImport("rths")] static extern void rthsRenderTargetSetup(IntPtr self, int width, int height, rthsRenderTargetFormat format);
        [DllImport("rths")] static extern void rthsRenderTargetSetOutputFormat(IntPtr self, rthsOutputFormat fmt);
        #endregion

        public static implicit operator bool(rthsRenderTarget v) { return v.self != IntPtr.Zero; }
        public static bool operator ==(rthsRenderTarget a, rthsRenderTarget b) { return a.self == b.self; }
        public static bool operator !=(rthsRenderTarget a, rthsRenderTarget b) { return a.self != b.self; }

        public string name
        {
            set { rthsRenderTargetSetName(self, value); }
        }
        public bool isRelocated
        {
            get { return rthsRenderTargetIsRelocated(self) != 0; }
        }
        public rthsOutputFormat outputFormat
        {
            set { rthsRenderTargetSetOutputFormat(self, value); }
        }

        public static rthsRenderTarget Create()
        {
            // rthsCreateRenderer() will return null if DXR is not supported
            return new rthsRenderTarget { self = rthsRenderTargetCreate() };
        }

        public void Release()
        {
            rthsRenderTargetRelease(self);
            self = IntPtr.Zero;
            rthsRenderer.IssueFlushDeferredCommands();
        }

        public void Setup(RenderTexture rtex)
        {
            if (rtex != null)
            {
                this.name = rtex.name;

                // if rtex is 32 bit int / uint format, set output format as bit masks
#if UNITY_2019_1_OR_NEWER
                switch (rtex.graphicsFormat)
                {
                    case GraphicsFormat.R32_UInt:
                    case GraphicsFormat.R32G32_UInt:
                    case GraphicsFormat.R32G32B32_UInt:
                    case GraphicsFormat.R32G32B32A32_UInt:
                    case GraphicsFormat.R32_SInt:
                    case GraphicsFormat.R32G32_SInt:
                    case GraphicsFormat.R32G32B32_SInt:
                    case GraphicsFormat.R32G32B32A32_SInt:
                        this.outputFormat = rthsOutputFormat.BitMask;
                        break;
                    default:
                        this.outputFormat = rthsOutputFormat.Float;
                        break;
                }
#else
                switch (rtex.format)
                {
                    case RenderTextureFormat.RInt:
                    case RenderTextureFormat.RGInt:
                    case RenderTextureFormat.ARGBInt:
                        this.outputFormat = rthsOutputFormat.BitMask;
                        break;
                    default:
                        this.outputFormat = rthsOutputFormat.Float;
                        break;
                }
#endif
                rthsRenderTargetSetGPUTexture(self, rtex.GetNativeTexturePtr());
            }
        }

        public void Setup(int width, int height, rthsRenderTargetFormat format)
        {
            rthsRenderTargetSetup(self, width, height, format);
        }
    }

    public struct rthsRenderer
    {
        #region internal
        public IntPtr self;
        [DllImport("rths")] static extern IntPtr rthsRendererCreate();
        [DllImport("rths")] static extern void rthsRendererRelease(IntPtr self);
        [DllImport("rths")] static extern byte rthsRendererIsInitialized(IntPtr self);
        [DllImport("rths")] static extern byte rthsRendererIsValid(IntPtr self);
        [DllImport("rths")] static extern int rthsRendererGetID(IntPtr self);
        [DllImport("rths")] static extern void rthsRendererSetName(IntPtr self, string name);

        [DllImport("rths")] static extern void rthsRendererBeginScene(IntPtr self);
        [DllImport("rths")] static extern void rthsRendererEndScene(IntPtr self);
        [DllImport("rths")] static extern void rthsRendererSetRenderFlags(IntPtr self, rthsRenderFlag flags);
        [DllImport("rths")] static extern void rthsRendererSetShadowRayOffset(IntPtr self, float v);
        [DllImport("rths")] static extern void rthsRendererSetSelfShadowThreshold(IntPtr self, float v);
        [DllImport("rths")] static extern void rthsRendererSetRenderTarget(IntPtr self, rthsRenderTarget rt);
        [DllImport("rths")] static extern void rthsRendererSetCamera(IntPtr self, Vector3 pos, Matrix4x4 view, Matrix4x4 proj, uint mask);
        [DllImport("rths")] static extern void rthsRendererAddDirectionalLight(IntPtr self, Vector3 dir, uint mask);
        [DllImport("rths")] static extern void rthsRendererAddSpotLight(IntPtr self, Vector3 pos, Vector3 dir, float range, float spotAngle, uint mask);
        [DllImport("rths")] static extern void rthsRendererAddPointLight(IntPtr self, Vector3 pos, float range, uint mask);
        [DllImport("rths")] static extern void rthsRendererAddReversePointLight(IntPtr self, Vector3 pos, float range, uint mask);
        [DllImport("rths")] static extern void rthsRendererAddMesh(IntPtr self, rthsMeshInstanceData mesh);
        [DllImport("rths")] static extern IntPtr rthsRendererGetTimestampLog(IntPtr self);

        [DllImport("rths")] static extern IntPtr rthsGetFlushDeferredCommands();
        [DllImport("rths")] static extern IntPtr rthsGetRenderAll();
        [DllImport("rths")] static extern IntPtr rthsGetMarkFrameBegin();
        [DllImport("rths")] static extern IntPtr rthsGetMarkFrameEnd();
        [DllImport("rths")] static extern IntPtr rthsGetRender();
        [DllImport("rths")] static extern IntPtr rthsGetFinish();
        #endregion

        public static implicit operator bool(rthsRenderer v) { return v.self != IntPtr.Zero; }
        public static bool operator ==(rthsRenderer a, rthsRenderer b) { return a.self == b.self; }
        public static bool operator !=(rthsRenderer a, rthsRenderer b) { return a.self != b.self; }


        public string name
        {
            set { rthsRendererSetName(self, value); }
        }
        public string timestampLog
        {
            get { return Misc.CString(rthsRendererGetTimestampLog(self)); }
        }
        public bool initialized
        {
            get { return rthsRendererIsInitialized(self) != 0; }
        }
        public bool valid
        {
            get { return rthsRendererIsValid(self) != 0; }
        }
        public int id
        {
            get { return rthsRendererGetID(self); }
        }

        public static rthsRenderer Create()
        {
            rthsGlobals.flags = rthsGlobalFlag.DeferredInitialization;
            var ret = new rthsRenderer { self = rthsRendererCreate() };
            IssueFlushDeferredCommands();
            return ret;
        }

        public void Release()
        {
            rthsRendererRelease(self);
            self = IntPtr.Zero;
            IssueFlushDeferredCommands();
        }

        public void SetRenderTarget(rthsRenderTarget rt)
        {
            rthsRendererSetRenderTarget(self, rt);
        }

        public void BeginScene()
        {
            rthsRendererBeginScene(self);
        }

        public void EndScene()
        {
            rthsRendererEndScene(self);
        }

        public void SetRaytraceFlags(rthsRenderFlag flags)
        {
            rthsRendererSetRenderFlags(self, flags);
        }
        public void SetShadowRayOffset(float v)
        {
            rthsRendererSetShadowRayOffset(self, v);
        }
        public void SetSelfShadowThreshold(float v)
        {
            rthsRendererSetSelfShadowThreshold(self, v);
        }

        public void SetCamera(Camera cam, bool useCullingMask = true)
        {
            uint mask = useCullingMask ? (uint)cam.cullingMask : ~0u;
            rthsRendererSetCamera(self, cam.transform.position, cam.worldToCameraMatrix, cam.projectionMatrix, mask);
        }

        public bool AddLight(Light light, bool useCullingMask = true)
        {
            uint mask = useCullingMask ? (uint)light.cullingMask : ~0u;
            var trans = light.transform;
            switch (light.type)
            {
                case LightType.Directional:
                    rthsRendererAddDirectionalLight(self, trans.forward, mask);
                    return true;
                case LightType.Spot:
                    rthsRendererAddSpotLight(self, trans.position, trans.forward, light.range, light.spotAngle, mask);
                    return true;
                case LightType.Point:
                    rthsRendererAddPointLight(self, trans.position, light.range, mask);
                    return true;
                default:
                    Debug.LogWarning("rthsShadowRenderer: " + light.type + " is not supported");
                    return false;
            }
        }
        public bool AddLight(ShadowCasterLight light)
        {
            var mask = 0xffffffff;
            var trans = light.transform;
            switch (light.lightType)
            {
                case ShadowCasterLightType.Directional:
                    rthsRendererAddDirectionalLight(self, trans.forward, mask);
                    return true;
                case ShadowCasterLightType.Spot:
                    rthsRendererAddSpotLight(self, trans.position, trans.forward, light.range, light.spotAngle, mask);
                    return true;
                case ShadowCasterLightType.Point:
                    rthsRendererAddPointLight(self, trans.position, light.range, mask);
                    return true;
                case ShadowCasterLightType.ReversePoint:
                    rthsRendererAddReversePointLight(self, trans.position, light.range, mask);
                    return true;
                default:
                    Debug.LogWarning("rthsShadowRenderer: " + light.lightType + " is not supported");
                    return false;
            }
        }

        public void AddMesh(rthsMeshInstanceData mesh)
        {
            rthsRendererAddMesh(self, mesh);
        }

        public static void IssueFlushDeferredCommands()
        {
            GL.IssuePluginEvent(rthsGetFlushDeferredCommands(), 0);
        }


        public void IssueMarkFrameBegin()
        {
            GL.IssuePluginEvent(rthsGetMarkFrameBegin(), 0);
        }
        public void IssueMarkFrameEnd()
        {
            GL.IssuePluginEvent(rthsGetMarkFrameEnd(), 0);
        }
        public void IssueRender()
        {
            GL.IssuePluginEvent(rthsGetRender(), this.id);
        }
        public void IssueFinish()
        {
            GL.IssuePluginEvent(rthsGetFinish(), this.id);
        }

        public void AddMarkFrameBegin(CommandBuffer cb)
        {
            cb.IssuePluginEvent(rthsGetMarkFrameBegin(), 0);
        }
        public void AddMarkFrameEnd(CommandBuffer cb)
        {
            cb.IssuePluginEvent(rthsGetMarkFrameEnd(), 0);
        }
        public void AddRender(CommandBuffer cb)
        {
            cb.IssuePluginEvent(rthsGetRender(), this.id);
        }
        public void AddFinish(CommandBuffer cb)
        {
            cb.IssuePluginEvent(rthsGetFinish(), this.id);
        }
    }

#pragma warning restore CS0660, CS0661
}
