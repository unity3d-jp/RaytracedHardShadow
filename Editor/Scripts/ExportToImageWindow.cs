using UnityEngine;
using UnityEditor;
using Unity.RaytracedHardShadow;

namespace Unity.RaytracedHardShadow.Editor
{
    internal class ExportToImageWindow : EditorWindow
    {
        ShadowRaytracer m_raytracer;
        static ShadowRaytracer.ImageFormat m_format = ShadowRaytracer.ImageFormat.PNG; // static to keep last selection
        string m_path;

        public static void Open(ShadowRaytracer sr)
        {
            var window = EditorWindow.GetWindow<ExportToImageWindow>();
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

            m_format = (ShadowRaytracer.ImageFormat)EditorGUILayout.EnumPopup("Format", m_format);
            if (GUILayout.Button("Export"))
            {
                string ext = "";
                switch(m_format)
                {
                    case ShadowRaytracer.ImageFormat.PNG: ext = "png"; break;
                    case ShadowRaytracer.ImageFormat.EXR: ext = "exr"; break;
#if UNITY_2018_3_OR_NEWER
                    case ShadowRaytracer.ImageFormat.TGA: ext = "tga"; break;
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

