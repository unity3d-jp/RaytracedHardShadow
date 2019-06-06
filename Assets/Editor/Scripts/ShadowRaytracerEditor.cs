using UnityEngine;
using UnityEditor;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    [CustomEditor(typeof(UTJ.RaytracedHardShadow.ShadowRaytracer))]
    public class ShadowRaytracerEditor : Editor
    {
        [MenuItem("GameObject/RaytracedHardShadow/Create ShadowRaytracer", false, 10)]
        public static void CreateMeshSyncServer(MenuCommand menuCommand)
        {
            var go = new GameObject();
            go.name = "ShadowRaytracer";
            var srt = go.AddComponent<ShadowRaytracer>();
            Undo.RegisterCreatedObjectUndo(go, "ShadowRaytracer");
        }

        public override void OnInspectorGUI()
        {
            DrawDefaultInspector();
        }
    }
}
