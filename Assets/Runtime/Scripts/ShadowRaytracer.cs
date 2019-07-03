using System;
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

                int indexStride = mesh.indexFormat == UnityEngine.Rendering.IndexFormat.UInt16 ? 2 : 4;
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
            public int useCount;

            public void Update(RenderTexture rtex)
            {
                if (!rtex.IsCreated())
                    rtex.Create();

                if (!rtData)
                {
                    rtData = rthsRenderTarget.Create();
                    rtData.Setup(rtex.GetNativeTexturePtr());
                }
            }

            public void Release()
            {
                rtData.Release();
                useCount = 0;
            }
        }

        #endregion


        #region fields
        public static readonly int kMaxLayers = 7;

        [SerializeField] bool m_generateRenderTexture = true;
        [SerializeField] RenderTexture m_outputTexture;
        [SerializeField] bool m_assignGlobalTexture = true;
        [SerializeField] string m_globalTextureName = "_RaytracedHardShadow";

        [SerializeField] bool m_cullBackFace = true;
        [SerializeField] bool m_ignoreSelfShadow = false;
        [SerializeField] bool m_keepSelfDropShadow = false;
        [SerializeField] float m_shadowRayOffset = 0.0001f;
        [SerializeField] float m_selfShadowThreshold = 0.001f;

        [SerializeField] bool m_GPUSkinning = true;

        // PlayerSettings is not available at runtime. so keep PlayerSettings.legacyClampBlendShapeWeights in this field
        [SerializeField] bool m_clampBlendshapeWeights = false;

        [Tooltip("Light scope for shadow geometries.")]
        [SerializeField] ObjectScope m_lightScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_lightScenes;
#endif
        [SerializeField] string[] m_lightScenePaths;
        [SerializeField] GameObject[] m_lightObjects;

        [Tooltip("Geometry scope for shadow geometries.")]
        [SerializeField] bool m_separateCastersAndReceivers;
        [SerializeField] ObjectScope m_geometryScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_geometryScenes;
#endif
        [SerializeField] string[] m_geometryScenePaths;
        [SerializeField] GameObject[] m_geometryObjects;
        [SerializeField] List<Layer> m_layers = new List<Layer> { new Layer() };

        rthsRenderer m_renderer;

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
        public bool cullBackFace
        {
            get { return m_cullBackFace; }
            set { m_cullBackFace = value; }
        }
        public bool GPUSkinning
        {
            get { return m_GPUSkinning; }
            set { m_GPUSkinning = value; }
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
                rec.Update(rt);
                s_renderTargetCache.Add(rt, rec);
            }
            rec.useCount++;
            return rec.rtData;
        }


        // user must call UpdateScenePaths() manually if made changes to layer
        public Layer GetLayer(int i)
        {
            return m_layers[i];
        }

        // user must call UpdateScenePaths() manually if made changes to layer
        public Layer AddLayer()
        {
            var ret = new Layer();
            m_layers.Add(ret);
            return ret;
        }
        public void RemoveLayer(Layer l)
        {
            m_layers.Remove(l);
        }

#if UNITY_EDITOR
        void UpdateScenePaths(ref string[] dst, SceneAsset[] src)
        {
            dst = Array.ConvertAll(src, s => s != null ? AssetDatabase.GetAssetPath(s) : null);
        }
#endif

        public void UpdateScenePaths()
        {
#if UNITY_EDITOR
            if (m_lightScope == ObjectScope.Scenes)
                UpdateScenePaths(ref m_lightScenePaths, m_lightScenes);

            if (m_separateCastersAndReceivers)
            {
                foreach(var layer in m_layers)
                {
                    if (layer.casterScope == ObjectScope.Scenes)
                        UpdateScenePaths(ref layer.casterScenePaths, layer.casterScenes);
                    if (layer.receiverScope == ObjectScope.Scenes)
                        UpdateScenePaths(ref layer.receiverScenePaths, layer.receiverScenes);
                }
            }
            else
            {
                if (m_geometryScope == ObjectScope.Scenes)
                    UpdateScenePaths(ref m_geometryScenePaths, m_geometryScenes);
            }
#endif
        }

        public void EnumerateLights(Action<Light> bodyL, Action<ShadowCasterLight> bodySCL)
        {
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

        public void EnumerateMeshRenderers(Action<MeshRenderer, byte, byte> bodyMR, Action<SkinnedMeshRenderer, byte, byte> bodySMR)
        {
            // C# 7.0 supports function in function but we stick to the old way for compatibility

            Action<GameObject[], byte, byte> processGOs = (gos, rmask, cmask) =>
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

            Action<string[], byte, byte> processScenes = (scenePaths, rmask, cmask) => {
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

            Action<byte, byte> processEntireScene = (rmask, cmask) =>
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
                    byte cmask = (byte)((uint)rthsHitMask.Caster << shift);
                    byte rmask = (byte)((uint)rthsHitMask.Rceiver | (uint)cmask);
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
                byte mask = (byte)rthsHitMask.Both;
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
                Debug.LogWarning("ShadowRaytracer: " + rthsRenderer.errorLog);
                this.enabled = false;
            }
        }
        #endregion


#if UNITY_EDITOR
        void Reset()
        {
        }

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
#if UNITY_EDITOR
            m_clampBlendshapeWeights = PlayerSettings.legacyClampBlendShapeWeights;
#endif
            InitializeRenderer();
            if (!m_renderer)
                return;

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

            bool updateGlobalTexture = false;
#if UNITY_EDITOR
            if (m_outputTexture != null && AssetDatabase.Contains(m_outputTexture))
            {
                if (!m_outputTexture.IsCreated())
                    m_outputTexture.Create();
                if (m_assignGlobalTexture)
                    updateGlobalTexture = true;
            }
            else
#endif
            if (m_generateRenderTexture)
            {
                // create output buffer if not assigned. fit its size to camera resolution if already assigned.

                var resolution = new Vector2Int(cam.pixelWidth, cam.pixelHeight);
                if (m_outputTexture != null && (m_outputTexture.width != resolution.x || m_outputTexture.height != resolution.y))
                {
                    m_outputTexture.Release();
                    m_outputTexture = null;
                }
                if (m_outputTexture == null)
                {
                    m_outputTexture = new RenderTexture(resolution.x, resolution.y, 0, RenderTextureFormat.RHalf);
                    m_outputTexture.name = "RaytracedHardShadow";
                    m_outputTexture.enableRandomWrite = true; // enable unordered access
                    m_outputTexture.Create();
                    if (m_assignGlobalTexture)
                        updateGlobalTexture = true;
                }
            }

            if (m_outputTexture == null)
                return;
            if (updateGlobalTexture)
                Shader.SetGlobalTexture(m_globalTextureName, m_outputTexture);

            {
                int flags = 0;
                if (m_cullBackFace)
                    flags |= (int)rthsRenderFlag.CullBackFace;
                if (m_ignoreSelfShadow)
                    flags |= (int)rthsRenderFlag.IgnoreSelfShadow;
                if (m_keepSelfDropShadow)
                    flags |= (int)rthsRenderFlag.KeepSelfDropShadow;
                if (m_GPUSkinning)
                    flags |= (int)rthsRenderFlag.GPUSkinning;
                if (m_clampBlendshapeWeights)
                    flags |= (int)rthsRenderFlag.ClampBlendShapeWights;

                m_renderer.BeginScene();
                m_renderer.SetRaytraceFlags(flags);
                m_renderer.SetShadowRayOffset(m_shadowRayOffset);
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
                // last instance issue render event.
                // all renderers do actual rendering tasks in render thread.
                rthsRenderer.IssueRender();
            }
        }
    }
}
