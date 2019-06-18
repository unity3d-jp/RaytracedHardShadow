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
        [SerializeField] bool m_GPUSkinning = false;

        [Tooltip("Light scope for shadow geometries.")]
        [SerializeField] ObjectScope m_lightScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_lightScenes;
#endif
        [SerializeField] GameObject[] m_lightObjects;

        [Tooltip("Geometry scope for shadow geometries.")]
        [SerializeField] ObjectScope m_geometryScope;
#if UNITY_EDITOR
        [SerializeField] SceneAsset[] m_geometryScenes;
#endif
        [SerializeField] GameObject[] m_geometryObjects;

        rthsRenderer m_renderer;

        static int s_instanceCount, s_updateCount;
        static Dictionary<Mesh, MeshRecord> s_meshDataCache, s_bakedMeshDataCache;
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


        public ObjectScope geometryScope
        {
            get { return m_geometryScope; }
            set { m_geometryScope = value; }
        }
#if UNITY_EDITOR
        public SceneAsset[] geometryScenes
        {
            get { return m_geometryScenes; }
            set { m_geometryScenes = value; }
        }
#endif
        public GameObject[] geometryObjects
        {
            get { return m_geometryObjects; }
            set { m_geometryObjects = value; }
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

        static void ClearAllMeshRecords()
        {
            foreach (var rec in s_meshDataCache)
                rec.Value.meshData.Release();
            s_meshDataCache.Clear();

            ClearBakedMeshRecords();
        }

        static void ClearBakedMeshRecords()
        {
            foreach (var rec in s_bakedMeshDataCache)
                rec.Value.meshData.Release();
            s_bakedMeshDataCache.Clear();
        }


        static rthsMeshData GetBakedMeshData(Mesh mesh)
        {
            if (mesh == null)
                return default(rthsMeshData);

            MeshRecord rec;
            if (!s_bakedMeshDataCache.TryGetValue(mesh, out rec))
            {
                rec = new MeshRecord();
                rec.Update(mesh);
                s_bakedMeshDataCache.Add(mesh, rec);
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

        rthsMeshInstanceData GetMeshInstanceData(MeshRenderer mr)
        {
            var mf = mr.GetComponent<MeshFilter>();
            var inst = rthsMeshInstanceData.Create(GetMeshData(mf.sharedMesh));
            inst.SetTransform(mr.localToWorldMatrix);
            return inst;
        }

        rthsMeshInstanceData GetMeshInstanceData(SkinnedMeshRenderer smr)
        {
            // bake is needed if there is Cloth, or skinned or has blendshapes and GPU skinning is disabled
            var cloth = smr.GetComponent<Cloth>();
            bool requireBake = cloth != null || (!m_GPUSkinning && (smr.rootBone != null || smr.sharedMesh.blendShapeCount != 0));
            if (requireBake)
            {
                var mesh = new Mesh();
                smr.BakeMesh(mesh);
                var inst = rthsMeshInstanceData.Create(GetBakedMeshData(mesh));
                inst.SetTransform(smr.localToWorldMatrix);
                return inst;
            }
            else
            {
                var md = GetMeshData(smr.sharedMesh);
                var inst = rthsMeshInstanceData.Create(md);
                inst.SetTransform(smr.localToWorldMatrix);
                inst.SetBones(smr);
                inst.SetBlendshapeWeights(smr);
                return inst;
            }
        }

        public void EnumerateMeshRenderers(Action<MeshRenderer> bodyMR, Action<SkinnedMeshRenderer> bodySMR)
        {
            if (m_geometryScope == ObjectScope.EntireScene)
            {
                foreach (var mr in FindObjectsOfType<MeshRenderer>())
                    if (mr.enabled)
                        bodyMR.Invoke(mr);
                foreach (var smr in FindObjectsOfType<SkinnedMeshRenderer>())
                    if (smr.enabled)
                        bodySMR.Invoke(smr);
            }
            else if (m_geometryScope == ObjectScope.SelectedScenes)
            {
#if UNITY_EDITOR
                int numScenes = SceneManager.sceneCount;
                for (int si = 0; si < numScenes; ++si)
                {
                    var scene = SceneManager.GetSceneAt(si);
                    if (!scene.isLoaded)
                        continue;

                    foreach (var sceneAsset in m_geometryScenes)
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

                                foreach (var mr in go.GetComponentsInChildren<MeshRenderer>())
                                    if (mr.enabled)
                                        bodyMR.Invoke(mr);
                                foreach (var smr in go.GetComponentsInChildren<SkinnedMeshRenderer>())
                                    if (smr.enabled)
                                        bodySMR.Invoke(smr);
                            }
                            break;
                        }
                    }
                }
#endif
            }
            else if (m_geometryScope == ObjectScope.SelectedObjects)
            {
                foreach (var go in m_geometryObjects)
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
            m_renderer = rthsRenderer.Create();
            if (m_renderer)
            {
                ++s_instanceCount;

                if (s_meshDataCache == null)
                {
                    s_meshDataCache = new Dictionary<Mesh, MeshRecord>();
                    s_bakedMeshDataCache = new Dictionary<Mesh, MeshRecord>();
                }
            }
            else
            {
                Debug.Log("ShadowRenderer: " + rthsRenderer.errorLog);
            }
        }

        void OnDisable()
        {
            ClearAllMeshRecords();
            if (m_renderer)
            {
                m_renderer.Release();
                --s_instanceCount;
            }
        }

        void Update()
        {
            // first instance reset count and clear cache
            if (s_updateCount != 0)
            {
                s_updateCount = 0;
                ClearBakedMeshRecords();
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
                if (m_ignoreSelfShadow)
                    flags |= (int)rthsRenderFlag.IgnoreSelfShadow;
                if (m_keepSelfDropShadow)
                    flags |= (int)rthsRenderFlag.KeepSelfDropShadow;
                if (m_GPUSkinning)
                    flags |= (int)rthsRenderFlag.GPUSkinning;
                if (PlayerSettings.legacyClampBlendShapeWeights)
                    flags |= (int)rthsRenderFlag.ClampBlendShapeWights;

                m_renderer.BeginScene();
                m_renderer.SetRaytraceFlags(flags);
                m_renderer.SetShadowRayOffset(m_shadowRayOffset);
                m_renderer.SetSelfShadowThreshold(m_selfShadowThreshold);
                m_renderer.SetRenderTarget(m_shadowBuffer);
                m_renderer.SetCamera(m_camera);
                EnumerateLights(
                    l => { m_renderer.AddLight(l); },
                    scl => { m_renderer.AddLight(scl); }
                    );
                EnumerateMeshRenderers(
                    mr => { m_renderer.AddMesh(GetMeshInstanceData(mr)); },
                    smr => { m_renderer.AddMesh(GetMeshInstanceData(smr)); }
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
