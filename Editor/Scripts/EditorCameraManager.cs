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

        static void RaytraceHardShadowPreRender(Camera cam)
        {
            if (!IsValidCamera(cam))
                return;

            var shadowRaytracer = GetEditorShadowRaytracer();
            if (shadowRaytracer == null) // No cameras has ShadowRaytracer in the game.
                return;

            shadowRaytracer.Render(cam);
        }


        static void RaytraceHardShadowPostRender(Camera cam)
        {
            if (!IsValidCamera(cam))
                return;

            var shadowRaytracer = GetEditorShadowRaytracer();
            if (shadowRaytracer == null) // No cameras has ShadowRaytracer in the game.
                return;

            shadowRaytracer.Finish();
        }

        //Only process Cameras in the Scene Window
        static bool IsValidCamera(Camera cam)
        {
            return (cam.cameraType == CameraType.SceneView)
                || (cam.cameraType == CameraType.Preview)
                || (cam.gameObject.name == "Preview Camera");
        }


        static ShadowRaytracer s_usedShadowRaytracerInGame = null;

        static ShadowRaytracer GetEditorShadowRaytracer()
        {
            if (s_usedShadowRaytracerInGame != null)
                return s_usedShadowRaytracerInGame;

            s_usedShadowRaytracerInGame = Object.FindObjectOfType<ShadowRaytracer>();
            return s_usedShadowRaytracerInGame;
        }
    }
} //namespace UTJ.RaytracedHardShadowEditor
