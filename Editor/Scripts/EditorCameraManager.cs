using UnityEditor;
using UnityEngine;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor {


    public static class EditorCameraManager {
    [InitializeOnLoadMethod]
    static void Init() {
        Camera.onPreRender += RaytraceHardShadowPreRender;
        Camera.onPostRender+= RaytraceHardShadowPostRender;
    }

//---------------------------------------------------------------------------------------------------------------------
    static void RaytraceHardShadowPreRender(Camera cam)
    {         
        if (!IsValidCamera(cam)) return;

        ShadowRaytracer shadowRaytracer = GetEditorShadowRaytracer();
        if (null==shadowRaytracer) //No cameras has ShadowRaytracer in the game.
            return;

        shadowRaytracer.Render(cam);
    }

//---------------------------------------------------------------------------------------------------------------------

    static void RaytraceHardShadowPostRender(Camera cam)
    {
        if (!IsValidCamera(cam)) return;

        ShadowRaytracer shadowRaytracer = GetEditorShadowRaytracer();
        if (null==shadowRaytracer) //No cameras has ShadowRaytracer in the game.
            return;

        shadowRaytracer.Finish();
    }

//---------------------------------------------------------------------------------------------------------------------
    //Only process Cameras in the Scene Window
    static bool IsValidCamera(Camera cam) {
        return (CameraType.SceneView == cam.cameraType)  
            || (CameraType.Preview== cam.cameraType)
            || ("Preview Camera" == cam.gameObject.name);
    }

//---------------------------------------------------------------------------------------------------------------------

    static ShadowRaytracer GetEditorShadowRaytracer()
    {
        if (null!=m_usedShadowRaytracerInGame)
            return m_usedShadowRaytracerInGame;

        m_usedShadowRaytracerInGame = Object.FindObjectOfType<ShadowRaytracer>();
        return m_usedShadowRaytracerInGame;
    }

//---------------------------------------------------------------------------------------------------------------------

    static ShadowRaytracer m_usedShadowRaytracerInGame = null;
}

} //namespace UTJ.RaytracedHardShadowEditor
