using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
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
                public bool updated = false;
            }

            public Dictionary<ShadowRaytracer, Record> records = new Dictionary<ShadowRaytracer, Record>();
        }
        static bool s_enableSceneViewRendering = true;
        static Dictionary<Camera, CameraContext> s_cameraContexts = new Dictionary<Camera, CameraContext>();


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

            CameraContext ctx;
            if (!s_cameraContexts.TryGetValue(cam, out ctx))
            {
                ctx = new CameraContext();
                s_cameraContexts.Add(cam, ctx);
            }

            foreach (var kvp in ctx.records)
                kvp.Value.updated = false;

            int updateCount = 0;
            foreach (var shadowRaytracer in GetEditorShadowRaytracer())
            {
                CameraContext.Record rec;
                if (!ctx.records.TryGetValue(shadowRaytracer, out rec))
                {
                    rec = new CameraContext.Record();
                    ctx.records.Add(shadowRaytracer, rec);
                }

                shadowRaytracer.Render(cam, ref rec.outputTexture, true);
                rec.updated = true;
                ++updateCount;
            }

            if (updateCount < ctx.records.Count)
            {
                // remove stale records
                var keys = ctx.records.Where(kvp => !kvp.Value.updated)
                    .Select(kvp => kvp.Key)
                    .ToList();
                foreach (var key in keys)
                    ctx.records.Remove(key);
            }
        }

        static void RaytraceHardShadowPostRender(Camera cam)
        {
            if (!s_enableSceneViewRendering || !IsValidCamera(cam))
                return;

            foreach (var shadowRaytracer in GetEditorShadowRaytracer())
                shadowRaytracer.Finish();
        }
    }
} //namespace UTJ.RaytracedHardShadowEditor
