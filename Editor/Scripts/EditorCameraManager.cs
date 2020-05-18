using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;
using Unity.RaytracedHardShadow;

namespace Unity.RaytracedHardShadow.Editor
{
    public static class EditorCameraManager
    {
        public static bool enableSceneViewRendering
        {
            get { return s_enableSceneViewRendering; }
            set { s_enableSceneViewRendering = value; }
        }

        [InitializeOnLoadMethod]
        static void Init()
        {
            Camera.onPreRender += RaytraceHardShadowPreRender;
            Camera.onPostRender += RaytraceHardShadowPostRender;
        }

        class CameraContext
        {
            public class Record
            {
                public RenderTexture outputTexture;
            }

            public Dictionary<ShadowRaytracer, Record> records = new Dictionary<ShadowRaytracer, Record>();
            public int updateCount;
        }
        static bool s_enableSceneViewRendering = true;
        static Dictionary<Camera, CameraContext> s_cameraContexts = new Dictionary<Camera, CameraContext>();
        static int s_updateCount = 0;


        // Only process Cameras in the Scene Window
        static bool IsValidCamera(Camera cam)
        {
            return  cam.cameraType == CameraType.SceneView ||
                    cam.cameraType == CameraType.Preview ||
                    cam.gameObject.name == "Preview Camera";
        }

        static ShadowRaytracer[] GetEditorShadowRaytracer()
        {
            return Object.FindObjectsOfType<ShadowRaytracer>();
        }

        static void RaytraceHardShadowPreRender(Camera cam)
        {
            if (!s_enableSceneViewRendering || !IsValidCamera(cam))
                return;

            var ctx = Misc.GetOrAddValue(s_cameraContexts, cam);
            foreach (var shadowRaytracer in GetEditorShadowRaytracer())
            {
                var rec = Misc.GetOrAddValue(ctx.records, shadowRaytracer);
                shadowRaytracer.Render(cam, ref rec.outputTexture, true);
            }

            ++ctx.updateCount;
            if (ctx.updateCount % 128 == 0)
                Misc.RemoveNullKeys(ctx.records);
        }

        static void RaytraceHardShadowPostRender(Camera cam)
        {
            if (!s_enableSceneViewRendering || !IsValidCamera(cam))
                return;

            foreach (var shadowRaytracer in GetEditorShadowRaytracer())
                shadowRaytracer.Finish();

            ++s_updateCount;
            if (s_updateCount % 256 == 0)
                Misc.RemoveNullKeys(s_cameraContexts);
        }
    }
} //namespace UTJ.RaytracedHardShadowEditor
