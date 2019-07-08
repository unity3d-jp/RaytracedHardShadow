using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
#endif
#if UNITY_2019_1_OR_NEWER
using Unity.Collections;
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

        [Serializable]
        public class Layer
        {
#if UNITY_EDITOR
            public bool fold = true;
#endif
            public ObjectScope receiverScope;
            public ObjectScope casterScope;
#if UNITY_EDITOR
            public SceneAsset[] receiverScenes;
            public SceneAsset[] casterScenes;
#endif
            // keep scene paths in *ScenePaths fields to handle scene scope at runtime
            public string[] receiverScenePaths;
            public string[] casterScenePaths;
            public GameObject[] receiverObjects;
            public GameObject[] casterObjects;
        }

        public class MeshRecord
        {
            public rthsMeshData meshData;
            public Mesh bakedMesh;
            public int useCount;

            public void Update(Mesh mesh)
            {
                Release();

                meshData = rthsMeshData.Create();
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

            public void Update(rthsMeshData md, Matrix4x4 trans)
            {
                if (instData && meshData != md)
                    instData.Release();
                meshData = md;
                if (!instData)
                    instData = rthsMeshInstanceData.Create(md);
                instData.SetTransform(trans);
            }

            public void Update(rthsMeshData md, MeshRenderer mr)
            {
                Update(md, mr.localToWorldMatrix);
            }

            public void Update(rthsMeshData md, SkinnedMeshRenderer smr)
            {
                var bones = smr.bones;
                if (bones.Length > 0)
                {
                    // skinned
                    var rootBone = smr.rootBone;
                    var rootMatrix = rootBone != null ? rootBone.localToWorldMatrix : Matrix4x4.identity;
                    Update(md, rootMatrix);
                    instData.SetBones(bones);
                }
                else
                {
                    // non-skinned
                    Update(md, smr.localToWorldMatrix);
                }
                instData.SetBlendshapeWeights(smr);
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
            public IntPtr nativeTexturePtr;

            public int useCount;

            public void Update(RenderTexture rtex)
            {
                if (!rtex.IsCreated())
                    rtex.Create();

                var ptr = rtex.GetNativeTexturePtr();
                if (rtData)
                {
                    if (nativeTexturePtr != ptr)
                        rtData.Release();
                }
                if (!rtData)
                {
                    rtData = rthsRenderTarget.Create();
                    rtData.Setup(ptr);
                    nativeTexturePtr = ptr;
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

        [SerializeField] ObjectScope m_lightScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_lightScenes;
#endif
        [SerializeField] string[] m_lightScenePaths;
        [SerializeField] GameObject[] m_lightObjects;

        [SerializeField] bool m_separateCastersAndReceivers;
        [SerializeField] ObjectScope m_geometryScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_geometryScenes;
#endif
        [SerializeField] string[] m_geometryScenePaths;
        [SerializeField] GameObject[] m_geometryObjects;
        [SerializeField] List<Layer> m_layers = new List<Layer> { new Layer() };

        [SerializeField] bool m_GPUSkinning = true;
        [SerializeField] bool m_adaptiveSampling = false;
        [SerializeField] bool m_antialiasing = false;
        [SerializeField] bool m_parallelCommandList = false;
        // PlayerSettings is not available at runtime. so keep PlayerSettings.legacyClampBlendShapeWeights in this field
        [SerializeField] bool m_clampBlendshapeWeights = true;

        [SerializeField] bool m_dbgTimestamp = false;
        [SerializeField] bool m_dbgForceUpdateAS = false;

#if UNITY_EDITOR
#pragma warning disable CS0414
        [SerializeField] bool m_foldDebug = false;
#pragma warning restore CS0414 
#endif

        rthsRenderer m_renderer;
        List<ExportRequest> m_exportRequests;

        static int s_instanceCount, s_updateCount;
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

        public bool separateCastersAndReceivers
        {
            get { return m_separateCastersAndReceivers; }
            set { m_separateCastersAndReceivers = value; }
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
        public int layerCount
        {
            get { return m_layers.Count; }
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

        public bool dbgTimestamp
        {
            get { return m_dbgTimestamp; }
            set { m_dbgTimestamp = value; }
        }
        public bool dbgForceUpdateAS
        {
            get { return m_dbgForceUpdateAS; }
            set { m_dbgForceUpdateAS = value; }
        }
        public string timestampLog
        {
            get { return m_renderer.timestampLog; }
        }
        #endregion


        #region public methods
        // user must call UpdateScenePaths() manually if made changes to the layer
        public Layer GetLayer(int i)
        {
            return m_layers[i];
        }

        // user must call UpdateScenePaths() manually if made changes to the layer
        // fail if layer count is already max (kMaxLayers)
        public Layer AddLayer()
        {
            if (m_layers.Count == kMaxLayers)
                return null;

            var ret = new Layer();
            m_layers.Add(ret);
            return ret;
        }

        public bool RemoveLayer(Layer l)
        {
            return m_layers.Remove(l);
        }

        // serialize SceneAssets as string paths to use at runtime
        public void UpdateScenePaths()
        {
#if UNITY_EDITOR
            if (m_lightScope == ObjectScope.Scenes)
                ToPaths(ref m_lightScenePaths, m_lightScenes);

            if (m_separateCastersAndReceivers)
            {
                foreach (var layer in m_layers)
                {
                    if (layer.casterScope == ObjectScope.Scenes)
                        ToPaths(ref layer.casterScenePaths, layer.casterScenes);
                    if (layer.receiverScope == ObjectScope.Scenes)
                        ToPaths(ref layer.receiverScenePaths, layer.receiverScenes);
                }
            }
            else
            {
                if (m_geometryScope == ObjectScope.Scenes)
                    ToPaths(ref m_geometryScenePaths, m_geometryScenes);
            }
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
            foreach (var rec in s_bakedMeshDataCache)
                rec.Value.meshData.Release();
            s_bakedMeshDataCache.Clear();
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
                foreach(var k in s_meshesToErase)
                {
                    var rec = s_meshDataCache[k];
                    rec.Release();
                    s_meshDataCache.Remove(k);
                }
                s_meshesToErase.Clear();
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
                foreach (var k in s_instToErase)
                {
                    var rec = s_meshInstDataCache[k];
                    rec.Release();
                    s_meshInstDataCache.Remove(k);
                }
                s_instToErase.Clear();
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
                foreach (var k in s_rtToErase)
                {
                    var rec = s_renderTargetCache[k];
                    rec.Release();
                    s_renderTargetCache.Remove(k);
                }
                s_rtToErase.Clear();
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
                rec.Update(mesh);
                s_meshDataCache.Add(mesh, rec);
            }
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
                rec.Update(GetBakedMeshData(smr), smr.localToWorldMatrix);
            }
            else
            {
                rec.Update(GetMeshData(smr.sharedMesh), smr);
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
            Action<MeshRenderer, rthsHitMask, rthsHitMask> bodyMR,
            Action<SkinnedMeshRenderer, rthsHitMask, rthsHitMask> bodySMR)
        {
            Action<GameObject[], rthsHitMask, rthsHitMask> processGOs = (gos, rmask, cmask) =>
            {
                foreach (var go in gos)
                {
                    if (go == null || !go.activeInHierarchy)
                        continue;

                    foreach (var mr in go.GetComponentsInChildren<MeshRenderer>())
                        if (mr.enabled)
                            bodyMR.Invoke(mr, rmask, cmask);
                    foreach (var smr in go.GetComponentsInChildren<SkinnedMeshRenderer>())
                        if (smr.enabled)
                            bodySMR.Invoke(smr, rmask, cmask);
                }
            };

            Action<string[], rthsHitMask, rthsHitMask> processScenes = (scenePaths, rmask, cmask) => {
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
                            processGOs(scene.GetRootGameObjects(), rmask, cmask);
                            break;
                        }
                    }
                }
            };

            Action<rthsHitMask, rthsHitMask> processEntireScene = (rmask, cmask) =>
            {
                foreach (var mr in FindObjectsOfType<MeshRenderer>())
                    if (mr.enabled)
                        bodyMR.Invoke(mr, rmask, cmask);
                foreach (var smr in FindObjectsOfType<SkinnedMeshRenderer>())
                    if (smr.enabled)
                        bodySMR.Invoke(smr, rmask, cmask);
            };

            if (m_separateCastersAndReceivers)
            {
                int shift = 0;
                foreach (var layer in m_layers)
                {
                    var cmask = (rthsHitMask)((uint)rthsHitMask.Caster << shift);
                    var rmask = (rthsHitMask.Rceiver | cmask);
                    switch (layer.receiverScope)
                    {
                        case ObjectScope.EntireScene: processEntireScene(rmask, 0); break;
                        case ObjectScope.Scenes: processScenes(layer.receiverScenePaths, rmask, 0); break;
                        case ObjectScope.Objects: processGOs(layer.receiverObjects, rmask, 0); break;
                    }
                    switch (layer.casterScope)
                    {
                        case ObjectScope.EntireScene: processEntireScene(0, cmask); break;
                        case ObjectScope.Scenes: processScenes(layer.casterScenePaths, 0, cmask); break;
                        case ObjectScope.Objects: processGOs(layer.casterObjects, 0, cmask); break;
                    }
                    shift += 1;
                }
            }
            else
            {
                var mask = rthsHitMask.Both;
                switch (m_geometryScope)
                {
                    case ObjectScope.EntireScene: processEntireScene(mask, mask); break;
                    case ObjectScope.Scenes: processScenes(m_geometryScenePaths, mask, mask); break;
                    case ObjectScope.Objects: processGOs(m_geometryObjects, mask, mask); break;
                }
            }
        }

        void InitializeRenderer()
        {
            if (m_renderer)
                return;

#if UNITY_EDITOR
            // initializing renderer on scene load causes a crash in GI baking. so wait until GI bake is completed.
            if (Lightmapping.isRunning)
                return;
#endif

            m_renderer = rthsRenderer.Create();
            if (m_renderer)
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
                Debug.LogError("ShadowRaytracer: Initialization failed - " + rthsRenderer.errorLog);
                this.enabled = false;
            }
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
#endif

        void OnEnable()
        {
            InitializeRenderer();
        }

        void OnDisable()
        {
            if (m_renderer)
            {
                m_renderer.Release();
                --s_instanceCount;

                if (s_instanceCount==0)
                    ClearAllCacheRecords();
            }
        }

        void Update()
        {
#if UNITY_EDITOR && UNITY_2018_3_OR_NEWER
            m_clampBlendshapeWeights = PlayerSettings.legacyClampBlendShapeWeights;
#endif
            InitializeRenderer();
            if (!m_renderer)
                return;
            else if(!m_renderer.valid)
            {
                Debug.LogError("ShadowRaytracer: Error - " + rthsRenderer.errorLog);
                m_renderer.Release();
                this.enabled = false;
                return;
            }

            // first instance reset update count and clear cache
            if (s_updateCount != 0)
            {
                s_updateCount = 0;
                ClearBakedMeshRecords();
                EraseUnusedMeshRecords();
            }
        }

        void LateUpdate()
        {
            if (!m_renderer)
                return;

            var cam = GetComponent<Camera>();
            if (cam == null)
                return;

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
                return;

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
                if (m_dbgTimestamp)
                    flags |= rthsRenderFlag.DbgTimestamp;
                if (m_dbgForceUpdateAS)
                    flags |= rthsRenderFlag.DbgForceUpdateAS;

                m_renderer.BeginScene();
                m_renderer.SetRaytraceFlags(flags);
                m_renderer.SetShadowRayOffset(m_ignoreSelfShadow ? 0.0f : m_shadowRayOffset);
                m_renderer.SetSelfShadowThreshold(m_selfShadowThreshold);
                m_renderer.SetRenderTarget(GetRenderTargetData(m_outputTexture));
                m_renderer.SetCamera(cam);
                EnumerateLights(
                    l => { m_renderer.AddLight(l); },
                    scl => { m_renderer.AddLight(scl); }
                    );
                EnumerateMeshRenderers(
                    (mr, rmask, cmask) => { m_renderer.AddGeometry(GetMeshInstanceData(mr), rmask, cmask); },
                    (smr, rmask, cmask) => { m_renderer.AddGeometry(GetMeshInstanceData(smr), rmask, cmask); }
                    );
                m_renderer.EndScene();
            }

            if (++s_updateCount == s_instanceCount)
            {
                // last instance issues render event.
                // all renderers do actual rendering tasks in render thread.
                rthsRenderer.IssueRender();
            }
        }
        #endregion
    }
}
