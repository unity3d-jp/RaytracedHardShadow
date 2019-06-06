using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace UTJ.RaytracedHardShadow
{
    [ExecuteInEditMode]
    public class ShadowRaytracer : MonoBehaviour
    {
#pragma warning disable 649
        [Tooltip("Output buffer. Must be R32F format.")]
        [SerializeField] RenderTexture m_shadowBuffer;

        [Tooltip("If this field is null, Camera.main will be used.")]
        [SerializeField] Camera m_camera;

        [Tooltip("Lights to cast shadow.")]
        [SerializeField] Light[] m_lights;

        [Tooltip("ShadowCasterLights to cast shadow.")]
        [SerializeField] ShadowCasterLight[] m_shadowCasterLights;

        rthsShadowRenderer m_renderer;
#pragma warning restore 649


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
            if (!m_renderer || m_shadowBuffer == null)
                return;
            var cam = m_camera != null ? m_camera : Camera.main;
            if (cam == null)
                return;

            int numLights = 0;
            if (m_lights != null)
                numLights += m_lights.Length;
            if (m_shadowCasterLights != null)
                numLights += m_shadowCasterLights.Length;
            if (numLights == 0)
                return;

            if (!m_shadowBuffer.IsCreated())
                m_shadowBuffer.Create();

            m_renderer.BeginScene();
            m_renderer.SetRenderTarget(m_shadowBuffer);
            m_renderer.SetCamera(cam);
            if (m_lights != null)
                foreach (var light in m_lights)
                    m_renderer.AddLight(light);
            if (m_shadowCasterLights != null)
                foreach (var light in m_shadowCasterLights)
                    m_renderer.AddLight(light);
            foreach (var mr in FindObjectsOfType<MeshRenderer>())
                m_renderer.AddMesh(mr);
            foreach (var smr in FindObjectsOfType<SkinnedMeshRenderer>())
                m_renderer.AddMesh(smr);
            m_renderer.EndScene();

            m_renderer.Render();
            m_renderer.Finish();
        }
    }
}
