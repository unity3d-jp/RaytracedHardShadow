using UnityEngine;
using UnityEditor;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    [CustomEditor(typeof(UTJ.RaytracedHardShadow.ShadowCasterLight))]
    public class ShadowCasterLightEditor : Editor
    {
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
