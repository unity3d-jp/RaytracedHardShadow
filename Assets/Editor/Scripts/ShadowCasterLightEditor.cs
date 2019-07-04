using UnityEngine;
using UnityEditor;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    [CustomEditor(typeof(UTJ.RaytracedHardShadow.ShadowCasterLight))]
    public class ShadowCasterLightEditor : Editor
    {
        [MenuItem("GameObject/RaytracedHardShadow/Create Shadow Caster Light", false, 10)]
        public static void CreateShadowCasterLight(MenuCommand menuCommand)
        {
            var go = new GameObject();
            go.name = "Shadow Caster Light";
            go.AddComponent<ShadowCasterLight>();
            Undo.RegisterCreatedObjectUndo(go, "ShadowRaytracer");
        }

        public override void OnInspectorGUI()
        {
            //DrawDefaultInspector();

            var t = target as ShadowCasterLight;
            var so = serializedObject;

            EditorGUILayout.PropertyField(so.FindProperty("m_lightType"));
            EditorGUILayout.Space();
            if (t.lightType == ShadowCasterLightType.Spot || t.lightType == ShadowCasterLightType.Point || t.lightType == ShadowCasterLightType.ReversePoint)
                EditorGUILayout.PropertyField(so.FindProperty("m_range"));
            if (t.lightType == ShadowCasterLightType.Spot)
                EditorGUILayout.PropertyField(so.FindProperty("m_spotAngle"));
            so.ApplyModifiedProperties();
        }
    }
}
