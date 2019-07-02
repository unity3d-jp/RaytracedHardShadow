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
            SelectedScenes,
            SelectedObjects,
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
        [SerializeField] RenderTexture m_shadowBuffer;
        [SerializeField] string m_globalTextureName = "_RaytracedHardShadow";
        [SerializeField] bool m_generateShadowBuffer = true;

        [SerializeField] Camera m_camera;

        [SerializeField] bool m_ignoreSelfShadow = false;
        [SerializeField] bool m_keepSelfDropShadow = false;
        [SerializeField] float m_shadowRayOffset = 0.0001f;
        [SerializeField] float m_selfShadowThreshold = 0.001f;
        [SerializeField] bool m_cullBackFace = true;
        [SerializeField] bool m_GPUSkinning = true;

        // PlayerSettings is not available at runtime. so keep PlayerSettings.legacyClampBlendShapeWeights in this field
        [SerializeField] bool m_clampBlendshapeWeights = false;

        [Tooltip("Light scope for shadow geometries.")]
        [SerializeField] ObjectScope m_lightScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_lightScenes;
#endif
        [SerializeField] GameObject[] m_lightObjects;

        [Tooltip("Geometry scope for shadow geometries.")]
        [SerializeField] bool m_separateCastersAndReceivers;
        [SerializeField] ObjectScope m_geometryScope;
        [SerializeField] ObjectScope m_receiverScope;
        [SerializeField] ObjectScope m_casterScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_geometryScenes;
        [SerializeField] SceneAsset[] m_receiverScenes;
        [SerializeField] SceneAsset[] m_casterScenes;
#endif
        [SerializeField] GameObject[] m_geometryObjects;
        [SerializeField] GameObject[] m_receiverObjects;
        [SerializeField] GameObject[] m_casterObjects;

        rthsRenderer m_renderer;

        static int s_instanceCount, s_updateCount;
        static Dictionary<Mesh, MeshRecord> s_meshDataCache;
        static Dictionary<Component, MeshRecord> s_bakedMeshDataCache;
        static Dictionary<Component, MeshInstanceRecord> s_meshInstDataCache;
        static Dictionary<RenderTexture, RenderTargetRecord> s_renderTargetCache;
        #endregion


        #region properties
        public RenderTexture shadowBuffer
        {
            get { return m_shadowBuffer; }
            set { m_shadowBuffer = value; }
        }
        public string globalTextureName
        {
            get { return m_globalTextureName; }
            set { m_globalTextureName = value; }
        }
        public bool autoGenerateShadowBuffer
        {
            get { return m_generateShadowBuffer; }
            set { m_generateShadowBuffer = value; }
        }

        public new Camera camera
        {
            get { return m_camera; }
            set { m_camera = value; }
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
            set { m_lightScenes = value; }
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
        public ObjectScope receiverScope
        {
            get { return m_receiverScope; }
            set { m_receiverScope = value; }
        }
        public ObjectScope casterScope
        {
            get { return m_casterScope; }
            set { m_casterScope = value; }
        }
#if UNITY_EDITOR
        public SceneAsset[] geometryScenes
        {
            get { return m_geometryScenes; }
            set { m_geometryScenes = value; }
        }
        public SceneAsset[] receiverScenes
        {
            get { return m_receiverScenes; }
            set { m_receiverScenes = value; }
        }
        public SceneAsset[] casterScenes
        {
            get { return m_casterScenes; }
            set { m_casterScenes = value; }
        }
#endif
        public GameObject[] geometryObjects
        {
            get { return m_geometryObjects; }
            set { m_geometryObjects = value; }
        }
        public GameObject[] receiverObjects
        {
            get { return m_receiverObjects; }
            set { m_receiverObjects = value; }
        }
        public GameObject[] casterObjects
        {
            get { return m_casterObjects; }
            set { m_casterObjects = value; }
        }
        #endregion


        #region impl
        public void EnumerateLights(Action<Light> bodyL, Action<ShadowCasterLight> bodySCL)
        {
            if (m_lightScope == ObjectScope.EntireScene)
            {
                foreach (var light in FindObjectsOfType<Light>())
                    if (light.enabled)
                        bodyL.Invoke(light);
                foreach (var slight in FindObjectsOfType<ShadowCasterLight>())
                    if (slight.enabled)
                        bodySCL.Invoke(slight);
            }
            else if (m_lightScope == ObjectScope.SelectedScenes)
            {
#if UNITY_EDITOR
                int numScenes = SceneManager.sceneCount;
                for (int si = 0; si < numScenes; ++si)
                {
                    var scene = SceneManager.GetSceneAt(si);
                    if (!scene.isLoaded)
                        continue;

                    foreach (var sceneAsset in m_lightScenes)
                    {
                        if (sceneAsset == null)
                            continue;

                        var path = AssetDatabase.GetAssetPath(sceneAsset);
                        if (scene.path == path)
                        {
                            foreach (var go in scene.GetRootGameObjects())
                            {
                                if (!go.activeInHierarchy)
                                    continue;

                                foreach (var light in go.GetComponentsInChildren<Light>())
                                    if (light.enabled)
                                        bodyL.Invoke(light);
                                foreach (var slight in go.GetComponentsInChildren<ShadowCasterLight>())
                                    if (slight.enabled)
                                        bodySCL.Invoke(slight);
                            }
                            break;
                        }
                    }
                }
#endif
            }
            else if (m_lightScope == ObjectScope.SelectedObjects)
            {
                foreach (var go in m_lightObjects)
                {
                    if (go == null || !go.activeInHierarchy)
                        continue;

                    foreach (var light in go.GetComponentsInChildren<Light>())
                        if (light.enabled)
                            bodyL.Invoke(light);
                    foreach (var slight in go.GetComponentsInChildren<ShadowCasterLight>())
                        if (slight.enabled)
                            bodySCL.Invoke(slight);
                }
            }
        }


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

        public void EnumerateMeshRenderers(Action<MeshRenderer, byte> bodyMR, Action<SkinnedMeshRenderer, byte> bodySMR)
        {
            // C# 7.0 supports function in function but we stick to the old way for compatibility

            Action<GameObject[], byte> processGOs = (gos, mask) =>
            {
                foreach (var go in gos)
                {
                    if (!go.activeInHierarchy)
                        continue;

                    foreach (var mr in go.GetComponentsInChildren<MeshRenderer>())
                        if (mr.enabled)
                            bodyMR.Invoke(mr, mask);
                    foreach (var smr in go.GetComponentsInChildren<SkinnedMeshRenderer>())
                        if (smr.enabled)
                            bodySMR.Invoke(smr, mask);
                }
            };

#if UNITY_EDITOR
            Action<SceneAsset[], byte> processScenes = (sceneAssets, mask) => {
                foreach (var sceneAsset in sceneAssets)
                {
                    if (sceneAsset == null)
                        return;

                    var path = AssetDatabase.GetAssetPath(sceneAsset);
                    int numScenes = SceneManager.sceneCount;
                    for (int si = 0; si < numScenes; ++si)
                    {
                        var scene = SceneManager.GetSceneAt(si);
                        if (scene.isLoaded && scene.path == path)
                        {
                            processGOs(scene.GetRootGameObjects(), mask);
                            break;
                        }
                    }
                }
            };
#endif

            Action<byte> processEntireScene = (mask) =>
            {
                foreach (var mr in FindObjectsOfType<MeshRenderer>())
                    if (mr.enabled)
                        bodyMR.Invoke(mr, mask);
                foreach (var smr in FindObjectsOfType<SkinnedMeshRenderer>())
                    if (smr.enabled)
                        bodySMR.Invoke(smr, mask);
            };

            if (m_separateCastersAndReceivers)
            {
                switch(m_receiverScope)
                {
                    case ObjectScope.EntireScene: processEntireScene((byte)rthsHitMask.Rceiver); break;
#if UNITY_EDITOR
                    case ObjectScope.SelectedScenes: processScenes(m_receiverScenes, (byte)rthsHitMask.Rceiver); break;
#endif
                    case ObjectScope.SelectedObjects: processGOs(m_receiverObjects, (byte)rthsHitMask.Rceiver); break;
                }
                switch (m_casterScope)
                {
                    case ObjectScope.EntireScene: processEntireScene((byte)rthsHitMask.Caster); break;
#if UNITY_EDITOR
                    case ObjectScope.SelectedScenes: processScenes(m_casterScenes, (byte)rthsHitMask.Caster); break;
#endif
                    case ObjectScope.SelectedObjects: processGOs(m_casterObjects, (byte)rthsHitMask.Caster); break;
                }
            }
            else
            {
                switch (m_geometryScope)
                {
                    case ObjectScope.EntireScene: processEntireScene((byte)rthsHitMask.All); break;
#if UNITY_EDITOR
                    case ObjectScope.SelectedScenes: processScenes(m_geometryScenes, (byte)rthsHitMask.All); break;
#endif
                    case ObjectScope.SelectedObjects: processGOs(m_geometryObjects, (byte)rthsHitMask.All); break;
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
            m_camera = GetComponent<Camera>();
            if (m_camera == null)
                m_camera = Camera.main;
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

            if (m_camera == null)
            {
                m_camera = Camera.main;
                if (m_camera == null)
                {
                    Debug.LogWarning("ShadowRaytracer: camera is null");
                }
            }

#if UNITY_EDITOR
            if (m_shadowBuffer != null && AssetDatabase.Contains(m_shadowBuffer))
            {
                if (!m_shadowBuffer.IsCreated())
                    m_shadowBuffer.Create();
                if (m_globalTextureName != null && m_globalTextureName.Length != 0)
                    Shader.SetGlobalTexture(m_globalTextureName, m_shadowBuffer);
            }
            else
#endif
            if (m_generateShadowBuffer)
            {
                // create output buffer if not assigned. fit its size to camera resolution if already assigned.

                var resolution = new Vector2Int(m_camera.pixelWidth, m_camera.pixelHeight);
                if (m_shadowBuffer != null && (m_shadowBuffer.width != resolution.x || m_shadowBuffer.height != resolution.y))
                {
                    m_shadowBuffer.Release();
                    m_shadowBuffer = null;
                }
                if (m_shadowBuffer == null)
                {
                    m_shadowBuffer = new RenderTexture(resolution.x, resolution.y, 0, RenderTextureFormat.RHalf);
                    m_shadowBuffer.name = "RaytracedHardShadow";
                    m_shadowBuffer.enableRandomWrite = true; // enable unordered access
                    m_shadowBuffer.Create();
                    if (m_globalTextureName != null && m_globalTextureName.Length != 0)
                        Shader.SetGlobalTexture(m_globalTextureName, m_shadowBuffer);
                }
            }
            if (m_shadowBuffer == null)
            {
                Debug.LogWarning("ShadowRaytracer: output ShadowBuffer is null");
            }

            if (m_camera != null && m_shadowBuffer != null)
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
                m_renderer.SetRenderTarget(GetRenderTargetData(m_shadowBuffer));
                m_renderer.SetCamera(m_camera);
                EnumerateLights(
                    l => { m_renderer.AddLight(l); },
                    scl => { m_renderer.AddLight(scl); }
                    );
                EnumerateMeshRenderers(
                    (mr, mask) => { m_renderer.AddGeometry(GetMeshInstanceData(mr), mask); },
                    (smr, mask) => { m_renderer.AddGeometry(GetMeshInstanceData(smr), mask); }
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
