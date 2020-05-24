using UnityEngine;
using UnityEditor;
using Unity.RaytracedHardShadow;

namespace Unity.RaytracedHardShadowEditor
{
    public class ExportToImageWindowSRP : EditorWindow
    {
        ShadowRaytracerSRP m_raytracer;
        static ShadowRaytracerSRP.ImageFormat m_format = ShadowRaytracerSRP.ImageFormat.PNG; // static to keep last selection
        string m_path;

        public static void Open(ShadowRaytracerSRP sr)
        {
            var window = EditorWindow.GetWindow<ExportToImageWindowSRP>();
            window.titleContent = new GUIContent("Export To Image");
            window.m_raytracer = sr;
            window.Show();
        }

        private void OnGUI()
        {
            if (m_raytracer == null)
            {
                Close();
                return;
            }

            m_format = (ShadowRaytracerSRP.ImageFormat)EditorGUILayout.EnumPopup("Format", m_format);
            if (GUILayout.Button("Export"))
            {
                string ext = "";
                switch(m_format)
                {
                    case ShadowRaytracerSRP.ImageFormat.PNG: ext = "png"; break;
                    case ShadowRaytracerSRP.ImageFormat.EXR: ext = "exr"; break;
#if UNITY_2018_3_OR_NEWER
                    case ShadowRaytracerSRP.ImageFormat.TGA: ext = "tga"; break;
#endif
                }

                string path = EditorUtility.SaveFilePanel("Path to export", "", m_raytracer.outputTexture.name + "." + ext, ext);
                m_raytracer.ExportToImage(path, m_format);
                SceneView.RepaintAll();
                Close();
            }
        }
    }
}

