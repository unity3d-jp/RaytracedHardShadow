using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace UTJ.RaytracedHardShadow
{
    [ExecuteInEditMode]
    [RequireComponent(typeof(Camera))]
    [AddComponentMenu("UTJ/Raytraced Hard Shadow/Shadow Raytracer")]
    public class ShadowRaytracer : MonoBehaviour
    {
        #region types
        public enum ObjectScope
        {
            EntireScene,
            Scenes,
            Objects,
        }

        public enum ImageFormat
        {
            PNG,
            EXR,
#if UNITY_2018_3_OR_NEWER
            TGA,
#endif
        }

        public class MeshRecord
        {
            public rthsMeshData meshData;
            public Mesh bakedMesh;

            public int useCount;

            public void Update(Mesh mesh)
            {
                bool markDynamic = bakedMesh != null;

                if (meshData)
                {
                    if (meshData.isRelocated)
                    {
                        Release();
                        if (s_dbgVerboseLog)
                            Debug.Log(String.Format("Relocated Mesh \"{0}\"\n", mesh.name));
                        markDynamic = true;
                    }
                }
                if (!meshData)
                {
                    meshData = rthsMeshData.Create();
                    meshData.name = mesh.name;
                    if (markDynamic)
                        meshData.MarkDynamic();
                    meshData.SetGPUBuffers(mesh);
                    meshData.SetBindpose(mesh.bindposes);
#if UNITY_2019_1_OR_NEWER
                    meshData.SetSkinWeights(mesh.GetBonesPerVertex(), mesh.GetAllBoneWeights());
#else
                    meshData.SetSkinWeights(mesh.boneWeights);
#endif

                    int numBS = mesh.blendShapeCount;
                    meshData.SetBlendshapeCount(numBS);
                    if (numBS > 0)
                    {
                        var deltaPoints = new Vector3[mesh.vertexCount];
                        var deltaNormals = new Vector3[mesh.vertexCount];
                        var deltaTangents = new Vector3[mesh.vertexCount];
                        for (int bsi = 0; bsi < numBS; ++bsi)
                        {
                            int numFrames = mesh.GetBlendShapeFrameCount(bsi);
                            for (int fi = 0; fi < numFrames; ++fi)
                            {
                                float weight = mesh.GetBlendShapeFrameWeight(bsi, fi);
                                mesh.GetBlendShapeFrameVertices(bsi, fi, deltaPoints, deltaNormals, deltaTangents);
                                meshData.AddBlendshapeFrame(bsi, deltaPoints, weight);
                            }
                        }
                    }
                }
            }

            public void Release()
            {
                meshData.Release();
                useCount = 0;
            }
        }

        public class MeshInstanceRecord
        {
            public rthsMeshInstanceData instData;
            public rthsMeshData meshData;
            public int useCount;

            static rthsInstanceFlag ToFlags(bool receiveShadows, ShadowCastingMode mode)
            {
                rthsInstanceFlag ret = 0;
                if (receiveShadows)
                    ret |= rthsInstanceFlag.ReceiveShadows;
                switch (mode)
                {
                    case ShadowCastingMode.Off:
                        break;
                    case ShadowCastingMode.On:
                        ret |= rthsInstanceFlag.CastShadows;
                        ret |= rthsInstanceFlag.CullBackShadow;
                        break;
                    case ShadowCastingMode.TwoSided:
                        ret |= rthsInstanceFlag.CastShadows;
                        break;
                    case ShadowCastingMode.ShadowsOnly:
                        ret |= rthsInstanceFlag.ShadowsOnly;
                        ret |= rthsInstanceFlag.CastShadows;
                        break;
                }
                return ret;
            }

            public void Update(rthsMeshData md, rthsInstanceFlag flags, Matrix4x4 trans, GameObject go)
            {
                if (instData && meshData != md)
                    instData.Release();
                meshData = md;
                if (!instData)
                {
                    instData = rthsMeshInstanceData.Create(md);
                    instData.name = go.name;
                }
                instData.transform = trans;
                instData.flags = flags;
                instData.layer = go.layer;
            }

            public void Update(rthsMeshData md, MeshRenderer mr)
            {
                rthsInstanceFlag flags = ToFlags(mr.receiveShadows, mr.shadowCastingMode);
                Update(md, flags, mr.localToWorldMatrix, mr.gameObject);
            }

            public void Update(rthsMeshData md, SkinnedMeshRenderer smr, bool useDeformData)
            {
                rthsInstanceFlag flags = ToFlags(smr.receiveShadows, smr.shadowCastingMode);

                if (useDeformData)
                {
                    var bones = smr.bones;
                    if (bones.Length > 0)
                    {
                        // skinned
                        var rootBone = smr.rootBone;
                        var rootMatrix = rootBone != null ? rootBone.localToWorldMatrix : Matrix4x4.identity;
                        Update(md, flags, rootMatrix, smr.gameObject);
                        instData.SetBones(bones);
                    }
                    else
                    {
                        // non-skinned
                        Update(md, flags, smr.localToWorldMatrix, smr.gameObject);
                    }
                    instData.SetBlendshapeWeights(smr);
                }
                else
                {
                    Update(md, flags, smr.localToWorldMatrix, smr.gameObject);
                }
            }

            public void Release()
            {
                instData.Release();
                meshData = default(rthsMeshData); // Release() should not be called here
                useCount = 0;
            }
        }

        public class RenderTargetRecord
        {
            public rthsRenderTarget rtData;

            public int useCount;

            public void Update(RenderTexture rtex)
            {
                if (!rtex.IsCreated())
                    rtex.Create();

                if (rtData)
                {
                    if (rtData.isRelocated)
                    {
                        rtData.Release();
                        if (s_dbgVerboseLog)
                            Debug.Log(String.Format("Relocated RenderTexture \"{0}\"\n", rtex.name));
                    }
                }
                if (!rtData)
                {
                    rtData = rthsRenderTarget.Create();
                    rtData.name = rtex.name;
                    rtData.Setup(rtex.GetNativeTexturePtr());
                }
            }

            public void Release()
            {
                rtData.Release();
                useCount = 0;
            }
        }

        public class ExportRequest
        {
            public string path;
            public ImageFormat format;
        }
        #endregion


        #region fields
        public static readonly int kMaxLayers = 7;

        [SerializeField] bool m_generateRenderTexture = true;
        [SerializeField] RenderTexture m_outputTexture;
        [SerializeField] bool m_assignGlobalTexture = true;
        [SerializeField] string m_globalTextureName = "_RaytracedHardShadow";

        [SerializeField] bool m_cullBackFaces = true;
        [SerializeField] bool m_flipCasterFaces = false;
        [SerializeField] bool m_ignoreSelfShadow = true;
        [SerializeField] bool m_keepSelfDropShadow = true;
        [SerializeField] float m_selfShadowThreshold = 0.0001f;
        [SerializeField] float m_shadowRayOffset = 0.0001f;

        [SerializeField] bool m_useCameraCullingMask = true;
        [SerializeField] bool m_useLightCullingMask = true;

        [SerializeField] ObjectScope m_lightScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_lightScenes;
#endif
        [SerializeField] string[] m_lightScenePaths;
        [SerializeField] GameObject[] m_lightObjects;

        [SerializeField] ObjectScope m_geometryScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_geometryScenes;
#endif
        [SerializeField] string[] m_geometryScenePaths;
        [SerializeField] GameObject[] m_geometryObjects;

        [SerializeField] bool m_GPUSkinning = true;
        [SerializeField] bool m_adaptiveSampling = false;
        [SerializeField] bool m_antialiasing = false;
        [SerializeField] bool m_parallelCommandList = false;
        // PlayerSettings is not available at runtime. so keep PlayerSettings.legacyClampBlendShapeWeights in this field
        [SerializeField] bool m_clampBlendshapeWeights = true;

        [SerializeField] bool m_dbgVerboseLog = false;

#if UNITY_EDITOR
#pragma warning disable CS0414
        [SerializeField] bool m_foldDebug = false;
#pragma warning restore CS0414 
        bool m_isCompiling = false;
#endif

        rthsRenderer m_renderer;
        bool m_initialized = false;
        List<ExportRequest> m_exportRequests;

        static int s_instanceCount, s_updateCount, s_renderCount;
        static bool s_dbgVerboseLog = false;
        static Dictionary<Mesh, MeshRecord> s_meshDataCache;
        static Dictionary<Component, MeshRecord> s_bakedMeshDataCache;
        static Dictionary<Component, MeshInstanceRecord> s_meshInstDataCache;
        static Dictionary<RenderTexture, RenderTargetRecord> s_renderTargetCache;
        #endregion


        #region properties
        public bool generateRenderTexture
        {
            get { return m_generateRenderTexture; }
            set { m_generateRenderTexture = value; }
        }
        public RenderTexture outputTexture
        {
            get { return m_outputTexture; }
            set { m_outputTexture = value; }
        }
        public bool assignGlobalTexture
        {
            get { return m_assignGlobalTexture; }
            set { m_assignGlobalTexture = value; }
        }
        public string globalTextureName
        {
            get { return m_globalTextureName; }
            set { m_globalTextureName = value; }
        }

        public bool cullBackFaces
        {
            get { return m_cullBackFaces; }
            set { m_cullBackFaces = value; }
        }
        public bool flipCasterFaces
        {
            get { return m_flipCasterFaces; }
            set { m_flipCasterFaces = value; }
        }
        public bool ignoreSelfShadow
        {
            get { return m_ignoreSelfShadow; }
            set { m_ignoreSelfShadow = value; }
        }
        public bool keepSelfDropShadow
        {
            get { return m_keepSelfDropShadow; }
            set { m_keepSelfDropShadow = value; }
        }
        public float selfShadowThreshold
        {
            get { return m_selfShadowThreshold; }
            set { m_selfShadowThreshold = value; }
        }
        public float shadowRayOffset
        {
            get { return m_shadowRayOffset; }
            set { m_shadowRayOffset = value; }
        }

        public bool useCameraCullingMask
        {
            get { return m_useCameraCullingMask; }
            set { m_useCameraCullingMask = value; }
        }
        public bool useLightCullingMask
        {
            get { return m_useLightCullingMask; }
            set { m_useLightCullingMask = value; }
        }

        public ObjectScope lightScope
        {
            get { return m_lightScope; }
            set { m_lightScope = value; }
        }
#if UNITY_EDITOR
        public SceneAsset[] lightScenes
        {
            get { return m_lightScenes; }
            set { m_lightScenes = value; UpdateScenePaths(); }
        }
#else
        public string[] lightScenePaths
        {
            get { return m_lightScenePaths; }
            set { m_lightScenePaths = value; }
        }
#endif
        public GameObject[] lightObjects
        {
            get { return m_lightObjects; }
            set { m_lightObjects = value; }
        }

        public ObjectScope geometryScope
        {
            get { return m_geometryScope; }
            set { m_geometryScope = value; }
        }
#if UNITY_EDITOR
        public SceneAsset[] geometryScenes
        {
            get { return m_geometryScenes; }
            set { m_geometryScenes = value; UpdateScenePaths(); }
        }
#else
        public string[] geometryScenePaths
        {
            get { return m_geometryScenePaths; }
            set { m_geometryScenePaths = value; }
        }
#endif
        public GameObject[] geometryObjects
        {
            get { return m_geometryObjects; }
            set { m_geometryObjects = value; }
        }

        public bool GPUSkinning
        {
            get { return m_GPUSkinning; }
            set { m_GPUSkinning = value; }
        }
        public bool adaptiveSampling
        {
            get { return m_adaptiveSampling; }
            set { m_adaptiveSampling = value; }
        }
        public bool antialiasing
        {
            get { return m_antialiasing; }
            set { m_antialiasing = value; }
        }
        public bool parallelCommandList
        {
            get { return m_parallelCommandList; }
            set { m_parallelCommandList = value; }
        }

        public string timestampLog
        {
            get { return m_renderer.timestampLog; }
        }
        #endregion


        #region public methods
        // user must call UpdateScenePaths() manually if made changes to the layer
        // serialize SceneAssets as string paths to use at runtime
        public void UpdateScenePaths()
        {
#if UNITY_EDITOR
            if (m_lightScope == ObjectScope.Scenes)
                ToPaths(ref m_lightScenePaths, m_lightScenes);
            if (m_geometryScope == ObjectScope.Scenes)
                ToPaths(ref m_geometryScenePaths, m_geometryScenes);
#endif
        }

        // request export to image. actual export is done at the end of frame.
        public void ExportToImage(string path, ImageFormat format)
        {
            if (path == null || path.Length == 0)
                return;

            if (m_exportRequests == null)
                m_exportRequests = new List<ExportRequest>();
            m_exportRequests.Add(new ExportRequest { path = path, format = format });
            StartCoroutine(DoExportToImage());
        }
        #endregion


        #region impl
        // mesh cache serves two purposes:
        // 1. prevent multiple SkinnedMeshRenderer.Bake() if there are multiple ShadowRaytracers
        //    this is just for optimization.
        // 2. prevent unexpected GC
        //    without cache, temporary meshes created by SkinnedMeshRenderer.Bake() may be GCed and can cause error or crash in render thread.

        static void ClearAllCacheRecords()
        {
            ClearBakedMeshRecords();

            foreach (var rec in s_meshDataCache)
                rec.Value.meshData.Release();
            s_meshDataCache.Clear();

            foreach (var rec in s_meshInstDataCache)
                rec.Value.instData.Release();
            s_meshInstDataCache.Clear();

            foreach (var rec in s_renderTargetCache)
                rec.Value.rtData.Release();
            s_renderTargetCache.Clear();
        }

        static void ClearBakedMeshRecords()
        {
            if (s_bakedMeshDataCache.Count != 0)
            {
                foreach (var rec in s_bakedMeshDataCache)
                    rec.Value.meshData.Release();

                if (s_dbgVerboseLog)
                    Debug.Log(String.Format("{0} MeshData (baked meshes) erased\n", s_bakedMeshDataCache.Count));
                s_bakedMeshDataCache.Clear();
            }
        }

        static List<Mesh> s_meshesToErase;
        static List<Component> s_instToErase;
        static List<RenderTexture> s_rtToErase;

        static void EraseUnusedMeshRecords()
        {
            // mesh data
            {
                if (s_meshesToErase == null)
                    s_meshesToErase = new List<Mesh>();
                foreach (var rec in s_meshDataCache)
                {
                    if (rec.Value.useCount == 0)
                        s_meshesToErase.Add(rec.Key);
                    rec.Value.useCount = 0;
                }
                if (s_meshesToErase.Count != 0)
                {
                    foreach (var k in s_meshesToErase)
                    {
                        var rec = s_meshDataCache[k];
                        rec.Release();
                        s_meshDataCache.Remove(k);
                    }

                    if (s_dbgVerboseLog)
                        Debug.Log(String.Format("{0} MeshData erased\n", s_meshesToErase.Count));
                    s_meshesToErase.Clear();
                }
            }

            // mesh instance data
            {
                if (s_instToErase == null)
                    s_instToErase = new List<Component>();
                foreach (var rec in s_meshInstDataCache)
                {
                    if (rec.Value.useCount == 0)
                        s_instToErase.Add(rec.Key);
                    rec.Value.useCount = 0;
                }
                if (s_instToErase.Count != 0)
                {
                    foreach (var k in s_instToErase)
                    {
                        var rec = s_meshInstDataCache[k];
                        rec.Release();
                        s_meshInstDataCache.Remove(k);
                    }

                    if (s_dbgVerboseLog)
                        Debug.Log(String.Format("{0} MeshInstanceData erased\n", s_instToErase.Count));
                    s_instToErase.Clear();
                }
            }

            // render target data
            {
                if (s_rtToErase == null)
                    s_rtToErase = new List<RenderTexture>();
                foreach (var rec in s_renderTargetCache)
                {
                    if (rec.Value.useCount == 0)
                        s_rtToErase.Add(rec.Key);
                    rec.Value.useCount = 0;
                }
                if (s_rtToErase.Count != 0)
                {
                    foreach (var k in s_rtToErase)
                    {
                        var rec = s_renderTargetCache[k];
                        rec.Release();
                        s_renderTargetCache.Remove(k);
                    }

                    if (s_dbgVerboseLog)
                        Debug.Log(String.Format("{0} RenderTargetData erased\n", s_rtToErase.Count));
                    s_rtToErase.Clear();
                }
            }
        }

        static rthsMeshData GetBakedMeshData(SkinnedMeshRenderer smr)
        {
            if (smr == null || smr.sharedMesh == null)
                return default(rthsMeshData);

            MeshRecord rec;
            if (!s_bakedMeshDataCache.TryGetValue(smr, out rec))
            {
                rec = new MeshRecord();
                rec.bakedMesh = new Mesh();
                smr.BakeMesh(rec.bakedMesh);
                rec.Update(rec.bakedMesh);
                s_bakedMeshDataCache.Add(smr, rec);
            }
            rec.useCount++;
            return rec.meshData;
        }

        static rthsMeshData GetMeshData(Mesh mesh)
        {
            if (mesh == null)
                return default(rthsMeshData);

            MeshRecord rec;
            if (!s_meshDataCache.TryGetValue(mesh, out rec))
            {
                rec = new MeshRecord();
                s_meshDataCache.Add(mesh, rec);
            }
            rec.Update(mesh);
            rec.useCount++;
            return rec.meshData;
        }


        static MeshInstanceRecord GetInstanceRecord(Component component)
        {
            MeshInstanceRecord rec;
            if (!s_meshInstDataCache.TryGetValue(component, out rec))
            {
                rec = new MeshInstanceRecord();
                s_meshInstDataCache.Add(component, rec);
            }
            rec.useCount++;
            return rec;
        }

        static rthsMeshInstanceData GetMeshInstanceData(MeshRenderer mr)
        {
            var rec = GetInstanceRecord(mr);
            var mf = mr.GetComponent<MeshFilter>();
            rec.Update(GetMeshData(mf.sharedMesh), mr);
            return rec.instData;
        }

        rthsMeshInstanceData GetMeshInstanceData(SkinnedMeshRenderer smr)
        {
            var rec = GetInstanceRecord(smr);

            // bake is needed if there is Cloth, or skinned or has blendshapes and GPU skinning is disabled
            var cloth = smr.GetComponent<Cloth>();
            bool requireBake = cloth != null || (!m_GPUSkinning && (smr.rootBone != null || smr.sharedMesh.blendShapeCount != 0));
            if (requireBake)
            {
                rec.Update(GetBakedMeshData(smr), smr, false);
            }
            else
            {
                rec.Update(GetMeshData(smr.sharedMesh), smr, true);
            }
            return rec.instData;
        }

        static rthsRenderTarget GetRenderTargetData(RenderTexture rt)
        {
            if (rt == null)
                return default(rthsRenderTarget);

            RenderTargetRecord rec;
            if (!s_renderTargetCache.TryGetValue(rt, out rec))
            {
                rec = new RenderTargetRecord();
                s_renderTargetCache.Add(rt, rec);
            }
            rec.Update(rt);
            rec.useCount++;
            return rec.rtData;
        }


#if UNITY_EDITOR
        static void ToPaths(ref string[] dst, SceneAsset[] src)
        {
            dst = Array.ConvertAll(src, s => s != null ? AssetDatabase.GetAssetPath(s) : null);
        }
#endif

        static bool FeedErrorLog()
        {
            var errorLog = rthsGlobals.errorLog;
            if (errorLog.Length > 0)
            {
                Debug.LogError("ShadowRaytracer: " + errorLog);
                rthsGlobals.ClearErrorLog();
                return true;
            }
            return false;
        }

        public void EnumerateLights(Action<Light> bodyL, Action<ShadowCasterLight> bodySCL)
        {
            // C# 7.0 supports function in function but we stick to the old way for compatibility

            Action<GameObject[]> processGOs = (gos) =>
            {
                foreach (var go in gos)
                {
                    if (go == null || !go.activeInHierarchy)
                        continue;

                    foreach (var l in go.GetComponentsInChildren<Light>())
                        if (l.enabled)
                            bodyL.Invoke(l);
                    foreach (var scl in go.GetComponentsInChildren<ShadowCasterLight>())
                        if (scl.enabled)
                            bodySCL.Invoke(scl);
                }
            };

            Action<string[]> processScenes = (scenePaths) =>
            {
                foreach (var scenePath in scenePaths)
                {
                    if (scenePath == null || scenePath.Length == 0)
                        continue;

                    int numScenes = SceneManager.sceneCount;
                    for (int si = 0; si < numScenes; ++si)
                    {
                        var scene = SceneManager.GetSceneAt(si);
                        if (scene.isLoaded && scene.path == scenePath)
                        {
                            processGOs(scene.GetRootGameObjects());
                            break;
                        }
                    }
                }
            };

            Action processEntireScene = () =>
            {
                foreach (var l in FindObjectsOfType<Light>())
                    if (l.enabled)
                        bodyL.Invoke(l);
                foreach (var scl in FindObjectsOfType<ShadowCasterLight>())
                    if (scl.enabled)
                        bodySCL.Invoke(scl);
            };

            switch (m_lightScope)
            {
                case ObjectScope.EntireScene: processEntireScene(); break;
                case ObjectScope.Scenes: processScenes(m_lightScenePaths); break;
                case ObjectScope.Objects: processGOs(m_lightObjects); break;
            }
        }

        public void EnumerateMeshRenderers(
            Action<MeshRenderer> bodyMR,
            Action<SkinnedMeshRenderer> bodySMR)
        {
            Action<GameObject[]> processGOs = (gos) =>
            {
                foreach (var go in gos)
                {
                    if (go == null || !go.activeInHierarchy)
                        continue;

                    foreach (var mr in go.GetComponentsInChildren<MeshRenderer>())
                        if (mr.enabled)
                            bodyMR.Invoke(mr);
                    foreach (var smr in go.GetComponentsInChildren<SkinnedMeshRenderer>())
                        if (smr.enabled)
                            bodySMR.Invoke(smr);
                }
            };

            Action<string[]> processScenes = (scenePaths) =>
            {
                foreach (var scenePath in scenePaths)
                {
                    if (scenePath == null || scenePath.Length == 0)
                        continue;

                    int numScenes = SceneManager.sceneCount;
                    for (int si = 0; si < numScenes; ++si)
                    {
                        var scene = SceneManager.GetSceneAt(si);
                        if (scene.isLoaded && scene.path == scenePath)
                        {
                            processGOs(scene.GetRootGameObjects());
                            break;
                        }
                    }
                }
            };

            Action processEntireScene = () =>
            {
                foreach (var mr in FindObjectsOfType<MeshRenderer>())
                    if (mr.enabled)
                        bodyMR.Invoke(mr);
                foreach (var smr in FindObjectsOfType<SkinnedMeshRenderer>())
                    if (smr.enabled)
                        bodySMR.Invoke(smr);
            };

            switch (m_geometryScope)
            {
                case ObjectScope.EntireScene: processEntireScene(); break;
                case ObjectScope.Scenes: processScenes(m_geometryScenePaths); break;
                case ObjectScope.Objects: processGOs(m_geometryObjects); break;
            }
        }

        // actual initialization is done in render thread (create d3d12 device if needed and initialize GPU resources).
        // wait for complete actual initialization if 'wait' is true.
        // if already initialized (or failed) it immediately returns its result. so calling this in every frame is no problem.
        bool InitializeRenderer(bool wait = false, double timeoutInSeconds = 3.0)
        {
            if (m_initialized)
                return m_renderer;
#if UNITY_EDITOR
            // initializing renderer can interfere GI baking. so wait until it is completed.
            if (Lightmapping.isRunning)
                return false;
#endif

            if (!m_renderer)
            {
                m_renderer = rthsRenderer.Create();
                m_renderer.name = gameObject.name;
                if (m_dbgVerboseLog)
                    Debug.Log(String.Format("Initializing Renderer start ({0}f)", Time.frameCount));
            }

            if (wait)
            {
                var freq = System.Diagnostics.Stopwatch.Frequency;
                var timeout = (long)(timeoutInSeconds * freq);
                var start = System.Diagnostics.Stopwatch.GetTimestamp();
                while (!m_renderer.initialized && (System.Diagnostics.Stopwatch.GetTimestamp() - start) < timeout)
                    System.Threading.Thread.Sleep(10);
                if (m_dbgVerboseLog)
                {
                    var elapsed = System.Diagnostics.Stopwatch.GetTimestamp() - start;
                    var elapsedMS = (double)elapsed / freq * 1000.0;
                    Debug.Log(String.Format("{0}ms waited", elapsedMS));
                }
            }

            if (m_renderer.initialized)
            {
                if (m_dbgVerboseLog)
                    Debug.Log(String.Format("Initializing Renderer finish ({0}f)", Time.frameCount));
                m_initialized = true;
                if (m_renderer.valid)
                {
                    ++s_instanceCount;

                    if (s_meshDataCache == null)
                    {
                        s_meshDataCache = new Dictionary<Mesh, MeshRecord>();
                        s_bakedMeshDataCache = new Dictionary<Component, MeshRecord>();
                        s_meshInstDataCache = new Dictionary<Component, MeshInstanceRecord>();
                        s_renderTargetCache = new Dictionary<RenderTexture, RenderTargetRecord>();
                    }
                }
                else
                {
                    m_renderer.Release();
                    FeedErrorLog();
                    this.enabled = false;
                }
            }
            return m_renderer;
        }

        void FinalizeRenderer()
        {
            if (m_initialized && m_renderer.valid)
            {
                --s_instanceCount;
                if (s_instanceCount == 0)
                {
                    s_dbgVerboseLog = m_dbgVerboseLog;
                    ClearAllCacheRecords();
                }
            }
            if (m_renderer)
            {
                //Debug.Log("Release: " + m_renderer.self);
                m_renderer.Release();
            }
            m_initialized = false;
            if (m_dbgVerboseLog)
                Debug.Log(String.Format("Finalize Renderer ({0}f)", Time.frameCount));
        }

        bool Render()
        {
            if (!m_initialized || !m_renderer)
                return false;
#if UNITY_EDITOR
            FeedErrorLog();
#endif

            var cam = GetComponent<Camera>();
            if (cam == null)
                return false;

            if (m_generateRenderTexture)
            {
                var resolution = new Vector2Int(cam.pixelWidth, cam.pixelHeight);
                if (m_outputTexture != null && (m_outputTexture.width != resolution.x || m_outputTexture.height != resolution.y))
                {
                    // resolution was changed. release existing RenderTexture
#if UNITY_EDITOR
                    if (!AssetDatabase.Contains(m_outputTexture))
#endif
                    {
                        DestroyImmediate(m_outputTexture);
                    }
                    m_outputTexture = null;
                }
                if (m_outputTexture == null)
                {
                    m_outputTexture = new RenderTexture(resolution.x, resolution.y, 0, RenderTextureFormat.RHalf);
                    m_outputTexture.name = "RaytracedHardShadow";
                    m_outputTexture.enableRandomWrite = true; // enable unordered access
                    m_outputTexture.Create();
                    if (m_assignGlobalTexture)
                        Shader.SetGlobalTexture(m_globalTextureName, m_outputTexture);
                }
            }
            else if (m_outputTexture != null)
            {
                if (m_assignGlobalTexture)
                    Shader.SetGlobalTexture(m_globalTextureName, m_outputTexture);
            }

            if (m_outputTexture == null)
                return false;

            {
                rthsRenderFlag flags = 0;
                if (m_cullBackFaces)
                    flags |= rthsRenderFlag.CullBackFaces;
                if (m_flipCasterFaces)
                    flags |= rthsRenderFlag.FlipCasterFaces;
                if (m_ignoreSelfShadow)
                    flags |= rthsRenderFlag.IgnoreSelfShadow;
                if (m_keepSelfDropShadow)
                    flags |= rthsRenderFlag.KeepSelfDropShadow;
                if (m_GPUSkinning)
                    flags |= rthsRenderFlag.GPUSkinning;
                if (m_adaptiveSampling)
                    flags |= rthsRenderFlag.AdaptiveSampling;
                if (m_antialiasing)
                    flags |= rthsRenderFlag.Antialiasing;
                if (m_parallelCommandList)
                    flags |= rthsRenderFlag.ParallelCommandList;
                if (m_clampBlendshapeWeights)
                    flags |= rthsRenderFlag.ClampBlendShapeWights;

                m_renderer.BeginScene();
                try
                {
                    m_renderer.SetRaytraceFlags(flags);
                    m_renderer.SetShadowRayOffset(m_ignoreSelfShadow ? 0.0f : m_shadowRayOffset);
                    m_renderer.SetSelfShadowThreshold(m_selfShadowThreshold);
                    m_renderer.SetRenderTarget(GetRenderTargetData(m_outputTexture));
                    m_renderer.SetCamera(cam, m_useCameraCullingMask);
                    EnumerateLights(
                        l => { m_renderer.AddLight(l, m_useLightCullingMask); },
                        scl => { m_renderer.AddLight(scl); }
                    );
                    EnumerateMeshRenderers(
                        (mr) => { m_renderer.AddMesh(GetMeshInstanceData(mr)); },
                        (smr) => { m_renderer.AddMesh(GetMeshInstanceData(smr)); }
                    );
                }
                catch (Exception e)
                {
                    Debug.LogError(e);
                }
                m_renderer.EndScene();
            }
            return true;
        }

        static Material s_matBlit;

        static bool DoExportToImage(RenderTexture texture, ExportRequest request)
        {
            if (texture == null || !texture.IsCreated() || request.path == null || request.path.Length == 0)
                return false;

            var f1 = RenderTextureFormat.ARGB32;
            var f2 = TextureFormat.ARGB32;
            var exrFlags = Texture2D.EXRFlags.CompressZIP;

            switch (texture.format)
            {
                case RenderTextureFormat.R8:
                case RenderTextureFormat.RG16:
                case RenderTextureFormat.ARGB32:
                    f1 = RenderTextureFormat.ARGB32;
                    f2 = TextureFormat.ARGB32;
                    break;

                case RenderTextureFormat.RHalf:
                case RenderTextureFormat.RGHalf:
                case RenderTextureFormat.ARGBHalf:
                    if (request.format == ImageFormat.EXR)
                    {
                        f1 = RenderTextureFormat.ARGBHalf;
                        f2 = TextureFormat.RGBAHalf;
                        exrFlags |= Texture2D.EXRFlags.OutputAsFloat;
                    }
                    else
                    {
                        f1 = RenderTextureFormat.ARGB32;
                        f2 = TextureFormat.ARGB32;
                    }
                    break;

                case RenderTextureFormat.RFloat:
                case RenderTextureFormat.RGFloat:
                case RenderTextureFormat.ARGBFloat:
                    if (request.format == ImageFormat.EXR)
                    {
                        f1 = RenderTextureFormat.ARGBFloat;
                        f2 = TextureFormat.RGBAFloat;
                        exrFlags |= Texture2D.EXRFlags.OutputAsFloat;
                    }
                    else
                    {
                        f1 = RenderTextureFormat.ARGB32;
                        f2 = TextureFormat.ARGB32;
                    }
                    break;
            }

            RenderTexture tmp1 = null;
            Texture2D tmp2 = null;
            bool ret = true;
            try
            {
                if (s_matBlit == null)
                    s_matBlit = new Material(Shader.Find("Hidden/UTJ/RaytracedHardShadow/Blit"));

                // ReadPixels() doesn't handle format conversion. so create intermediate RenderTexture and Blit() to it.
                tmp1 = new RenderTexture(texture.width, texture.height, 0, f1);
                tmp2 = new Texture2D(texture.width, texture.height, f2, false);
                Graphics.Blit(texture, tmp1, s_matBlit);
                RenderTexture.active = tmp1;
                tmp2.ReadPixels(new Rect(0, 0, tmp2.width, tmp2.height), 0, 0);
                tmp2.Apply();
                RenderTexture.active = null;

                switch (request.format)
                {
                    case ImageFormat.PNG: ExportToPNG(request.path, tmp2); break;
                    case ImageFormat.EXR: ExportToEXR(request.path, tmp2, exrFlags); break;
#if UNITY_2018_3_OR_NEWER
                    case ImageFormat.TGA: ExportToTGA(request.path, tmp2); break;
#endif
                    default: ret = false; break;
                }
            }
            catch (Exception e)
            {
                Debug.LogError(e);
                ret = false;
            }

            if (tmp1 != null)
                DestroyImmediate(tmp1);
            if (tmp2 != null)
                DestroyImmediate(tmp2);
            return ret;
        }
        static void ExportToPNG(string path, Texture2D tex)
        {
#if UNITY_2018_1_OR_NEWER
            System.IO.File.WriteAllBytes(path, ImageConversion.EncodeToPNG(tex));
#else
            System.IO.File.WriteAllBytes(path, tex.EncodeToPNG());
#endif
        }
        static void ExportToEXR(string path, Texture2D tex, Texture2D.EXRFlags flags)
        {
#if UNITY_2018_1_OR_NEWER
            System.IO.File.WriteAllBytes(path, ImageConversion.EncodeToEXR(tex, flags));
#else
            System.IO.File.WriteAllBytes(path, tex.EncodeToEXR(flags));
#endif
        }
#if UNITY_2018_3_OR_NEWER
        static void ExportToTGA(string path, Texture2D tex)
        {
            System.IO.File.WriteAllBytes(path, ImageConversion.EncodeToTGA(tex));
        }
#endif

        IEnumerator DoExportToImage()
        {
            yield return new WaitForEndOfFrame();

            if (m_outputTexture != null && m_outputTexture.IsCreated())
            {
                foreach (var request in m_exportRequests)
                    DoExportToImage(m_outputTexture, request);
            }
            m_exportRequests.Clear();
        }
        #endregion


        #region events
#if UNITY_EDITOR
        void OnValidate()
        {
            UpdateScenePaths();
        }

        void OnGUI()
        {
            if (!EditorApplication.isPlaying || EditorApplication.isPaused)
            {
                if (Event.current.type == EventType.Repaint)
                {
                    if (InitializeRenderer(true) && Render())
                        rthsRenderer.IssueRender();
                }
            }
        }
#endif

        void OnEnable()
        {
            InitializeRenderer(true);
        }

        void OnDisable()
        {
            FinalizeRenderer();
        }

        void Update()
        {
#if UNITY_EDITOR && UNITY_2018_3_OR_NEWER
            m_clampBlendshapeWeights = PlayerSettings.legacyClampBlendShapeWeights;
#endif
#if UNITY_EDITOR
            // handle script recompile
            if (EditorApplication.isCompiling && !m_isCompiling)
            {
                // on compile begin
                m_isCompiling = true;
                FinalizeRenderer();
            }
            else if (!EditorApplication.isCompiling && m_isCompiling)
            {
                // on compile end
                m_isCompiling = false;
            }
#endif
            InitializeRenderer();
            if (!m_initialized || !m_renderer)
                return;

            // first instance reset render count and clear cache
            if (s_updateCount++ == 0)
            {
                s_dbgVerboseLog = m_dbgVerboseLog;
                ClearBakedMeshRecords();
                EraseUnusedMeshRecords();
            }
        }

        void LateUpdate()
        {
            s_updateCount = 0;
            s_renderCount = 0;
        }

        void OnPreRender()
        {
            if (!m_initialized || !m_renderer)
                return;

            // note: on Editor, Update() and OnPreRender() is not 1 on 1.
            //       multiple OnPreRender() can happen because of rapaint event.

            Render();
            if (++s_renderCount == s_instanceCount)
            {
                // last instance issues render event.
                // all renderers do actual rendering tasks in render thread.
                rthsRenderer.IssueRender();
                s_renderCount = 0;
            }
        }
        #endregion
    }
}
