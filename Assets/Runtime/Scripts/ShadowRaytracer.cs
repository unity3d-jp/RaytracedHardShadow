using System;
using UnityEngine;
using UnityEngine.SceneManagement;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace UTJ.RaytracedHardShadow
{
    [ExecuteInEditMode]
    public class ShadowRaytracer : MonoBehaviour
    {
        public enum ObjectScope
        {
            EntireScene,
            SelectedScenes,
            SelectedObjects,
        }

        #region fields
        [SerializeField] RenderTexture m_shadowBuffer;
        [SerializeField] string m_globalTextureName = "_RaytracedHardShadow";
        [SerializeField] bool m_generateShadowBuffer = true;

        [SerializeField] Camera m_camera;

        [SerializeField] bool m_ignoreSelfShadow = false;
        [SerializeField] bool m_keepSelfDropShadow = false;

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

        rthsShadowRenderer m_renderer;
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
            m_renderer = rthsShadowRenderer.Create();
            if (!m_renderer)
            {
                Debug.Log("ShadowRenderer: " + rthsShadowRenderer.errorLog);
            }
        }

        void OnDisable()
        {
            m_renderer.Destroy();
        }

        void Update()
        {
            m_renderer.Update();
        }

        void LateUpdate()
        {
            if (!m_renderer)
            {
                return;
            }

            if (m_camera == null)
            {
                m_camera = Camera.main;
                if (m_camera == null)
                    return;
            }

#if UNITY_EDITOR
            // ignore asset RenderTexture
            if (m_shadowBuffer != null && UnityEditor.AssetDatabase.GetAssetPath(m_shadowBuffer).Length != 0)
            {
                if (m_shadowBuffer.format != RenderTextureFormat.RFloat)
                {
                    Debug.Log("ShadowRaytracer: ShadowBuffer's format must be RFloat");
                    return;
                }
                if (!m_shadowBuffer.IsCreated())
                    m_shadowBuffer.Create();
            }
            else
#endif
            if (m_generateShadowBuffer)
            {
                var resolution = new Vector2Int(m_camera.pixelWidth, m_camera.pixelHeight);
                if (m_shadowBuffer != null && (m_shadowBuffer.width != resolution.x || m_shadowBuffer.height != resolution.y))
                {
                    m_shadowBuffer.Release();
                    m_shadowBuffer = null;
                }
                if (m_shadowBuffer == null)
                {
                    m_shadowBuffer = new RenderTexture(resolution.x, resolution.y, 0, RenderTextureFormat.RFloat);
                    m_shadowBuffer.name = "RaytracedHardShadow";
                    m_shadowBuffer.enableRandomWrite = true; // enable unordered access
                    m_shadowBuffer.Create();
                    if (m_globalTextureName != null && m_globalTextureName.Length != 0)
                        Shader.SetGlobalTexture(m_globalTextureName, m_shadowBuffer);
                }
            }

            if (m_shadowBuffer == null)
            {
                Debug.Log("ShadowRaytracer: output ShadowBuffer is null");
                return;
            }

            int flags = 0;
            if (m_ignoreSelfShadow)
            {
                flags |= (int)rthsRaytraceFlags.IgnoreSelfShadow;
                if (m_keepSelfDropShadow)
                    flags |= (int)rthsRaytraceFlags.KeepSelfDropShadow;
            }

            m_renderer.BeginScene();
            m_renderer.SetRaytraceFlags(flags);
            m_renderer.SetRenderTarget(m_shadowBuffer);
            m_renderer.SetCamera(m_camera);
            EnumerateLights(
                l => { m_renderer.AddLight(l); },
                scl => { m_renderer.AddLight(scl); }
                );
            EnumerateMeshRenderers(
                mr => { m_renderer.AddMesh(mr); },
                smr => { m_renderer.AddMesh(smr); }
                );
            m_renderer.EndScene();

            m_renderer.Render();
            m_renderer.Finish();
        }
    }
}
