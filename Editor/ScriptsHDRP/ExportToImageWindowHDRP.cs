using UnityEngine;
using UnityEditor;
using Unity.RaytracedHardShadow;

namespace Unity.RaytracedHardShadowEditor
{
    public class ExportToImageWindowHDRP : EditorWindow
    {
        ShadowRaytracerHDRP m_raytracer;
        static ShadowRaytracerHDRP.ImageFormat m_format = ShadowRaytracerHDRP.ImageFormat.PNG; // static to keep last selection
        string m_path;

        public static void Open(ShadowRaytracerHDRP sr)
        {
            var window = EditorWindow.GetWindow<ExportToImageWindowHDRP>();
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

            m_format = (ShadowRaytracerHDRP.ImageFormat)EditorGUILayout.EnumPopup("Format", m_format);
            if (GUILayout.Button("Export"))
            {
                string ext = "";
                switch(m_format)
                {
                    case ShadowRaytracerHDRP.ImageFormat.PNG: ext = "png"; break;
                    case ShadowRaytracerHDRP.ImageFormat.EXR: ext = "exr"; break;
#if UNITY_2018_3_OR_NEWER
                    case ShadowRaytracerHDRP.ImageFormat.TGA: ext = "tga"; break;
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

