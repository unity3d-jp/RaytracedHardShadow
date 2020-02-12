using System.Collections.Generic;
using System.Linq;
using UnityEditor;
using UnityEngine;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    public static class EditorCameraManager
    {
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
        static Dictionary<Camera, CameraContext> s_cameraContexts = new Dictionary<Camera, CameraContext>();
        static int s_updateCount = 0;


        // Only process Cameras in the Scene Window
        static bool IsValidCamera(Camera cam)
        {
            return  cam.cameraType == CameraType.SceneView ||
                    cam.cameraType == CameraType.Preview ||
                    cam.gameObject.name == "Preview Camera";
        }

        static ShadowRaytracer[] GetEditorShadowRaytracers()
        {
            return Object.FindObjectsOfType<ShadowRaytracer>();
        }

//----------------------------------------------------------------------------------------------------------------------
        static void RaytraceHardShadowPreRender(Camera cam) {
            if (!IsValidCamera(cam))
                return;

            CameraContext  ctx = Misc.GetOrAddValue(s_cameraContexts, cam);
            m_curShadowRaytracers = GetEditorShadowRaytracers();
            foreach (ShadowRaytracer shadowRaytracer in m_curShadowRaytracers) {
                if (!shadowRaytracer.IsPreviewShownInSceneView())
                    continue;
                CameraContext.Record rec = Misc.GetOrAddValue(ctx.records, shadowRaytracer);
                shadowRaytracer.Render(cam, ref rec.outputTexture, true);
            }

            ++ctx.updateCount;
            if (ctx.updateCount % 128 == 0)
                Misc.RemoveNullKeys(ctx.records);
        }

//----------------------------------------------------------------------------------------------------------------------

        static void RaytraceHardShadowPostRender(Camera cam) {
            if (!IsValidCamera(cam))
                return;

            foreach (ShadowRaytracer shadowRaytracer in m_curShadowRaytracers) {
                if (!shadowRaytracer.IsPreviewShownInSceneView())
                    continue;

                shadowRaytracer.Finish();
            }

            ++s_updateCount;
            if (s_updateCount % 256 == 0)
                Misc.RemoveNullKeys(s_cameraContexts);
        }

//----------------------------------------------------------------------------------------------------------------------
        static UTJ.RaytracedHardShadow.ShadowRaytracer[] m_curShadowRaytracers;
    }
} //namespace UTJ.RaytracedHardShadowEditor
