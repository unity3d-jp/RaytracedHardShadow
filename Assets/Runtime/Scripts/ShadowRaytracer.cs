using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace UTJ.RaytracedHardShadow
{
    [ExecuteInEditMode]
    class ShadowRaytracer : MonoBehaviour
    {
#if UNITY_EDITOR
        [MenuItem("GameObject/RaytracedHardShadow/Create ShadowRaytracer", false, 10)]
        public static void CreateMeshSyncServer(MenuCommand menuCommand)
        {
            var go = new GameObject();
            go.name = "ShadowRaytracer";
            var srt = go.AddComponent<ShadowRaytracer>();
            Undo.RegisterCreatedObjectUndo(go, "ShadowRaytracer");
        }
#endif

#pragma warning disable 649
        [Tooltip("If this field is null, Camera.main will be used.")]
        [SerializeField] Camera m_camera;

        [Tooltip("Light(s) to cast shadow. Must be directional.")]
        [SerializeField] Light[] m_lights;

        [Tooltip("Output buffer. Must be R32F format.")]
        [SerializeField] RenderTexture m_shadowBuffer;

        rthsShadowRenderer m_renderer;
#pragma warning restore 649


        void Awake()
        {
            m_renderer = rthsShadowRenderer.Create();
            if (!m_renderer)
            {
                Debug.Log("ShadowRenderer: " + rthsShadowRenderer.errorLog);
            }
        }

        void OnDestroy()
        {
            m_renderer.Destroy();
        }

        void LateUpdate()
        {
            if (!m_renderer || m_shadowBuffer == null)
                return;
            var cam = m_camera != null ? m_camera : Camera.main;
            if (cam == null || m_lights.Length == 0)
                return;

            if (!m_shadowBuffer.IsCreated())
                m_shadowBuffer.Create();

            m_renderer.SetRenderTarget(m_shadowBuffer);
            m_renderer.BeginScene();
            m_renderer.SetCamera(cam);
            foreach (var light in m_lights)
                m_renderer.AddLight(light);
            foreach (var mr in FindObjectsOfType<MeshRenderer>())
                m_renderer.AddMesh(mr);
            foreach (var smr in FindObjectsOfType<SkinnedMeshRenderer>())
                m_renderer.AddMesh(smr);
            m_renderer.EndScene();
            m_renderer.Render();
        }
    }
}
