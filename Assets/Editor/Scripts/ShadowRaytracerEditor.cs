using UnityEngine;
using UnityEditor;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    [CustomEditor(typeof(UTJ.RaytracedHardShadow.ShadowRaytracer))]
    public class ShadowRaytracerEditor : Editor
    {
        public override void OnInspectorGUI()
        {
            //DrawDefaultInspector();

            var t = target as ShadowRaytracer;
            var so = serializedObject;

            EditorGUILayout.PropertyField(so.FindProperty("m_generateRenderTexture"));
            if (!t.generateRenderTexture)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.PropertyField(so.FindProperty("m_outputTexture"));
                EditorGUI.indentLevel--;
            }

            EditorGUILayout.PropertyField(so.FindProperty("m_assignGlobalTexture"));
            if (t.assignGlobalTexture)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.PropertyField(so.FindProperty("m_globalTextureName"));
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.Space();

            EditorGUILayout.PropertyField(so.FindProperty("m_ignoreSelfShadow"));
            if (t.ignoreSelfShadow)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.PropertyField(so.FindProperty("m_selfShadowThreshold"));
                EditorGUILayout.PropertyField(so.FindProperty("m_keepSelfDropShadow"));
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.PropertyField(so.FindProperty("m_shadowRayOffset"));
            EditorGUILayout.Space();

            EditorGUILayout.PropertyField(so.FindProperty("m_lightScope"));
            if (t.lightScope == ShadowRaytracer.ObjectScope.SelectedScenes)
                EditorGUILayout.PropertyField(so.FindProperty("m_lightScenes"), true);
            else if (t.lightScope == ShadowRaytracer.ObjectScope.SelectedObjects)
                EditorGUILayout.PropertyField(so.FindProperty("m_lightObjects"), true);
            EditorGUILayout.Space();

            EditorGUILayout.PropertyField(so.FindProperty("m_separateCastersAndReceivers"));
            if (t.separateCastersAndReceivers)
            {
                EditorGUILayout.PropertyField(so.FindProperty("m_receiverScope"));
                switch (t.receiverScope)
                {
                    case ShadowRaytracer.ObjectScope.SelectedScenes:
                        EditorGUILayout.PropertyField(so.FindProperty("m_receiverScenes"), true);
                        break;
                    case ShadowRaytracer.ObjectScope.SelectedObjects:
                        EditorGUILayout.PropertyField(so.FindProperty("m_receiverObjects"), true);
                        break;
                }

                EditorGUILayout.PropertyField(so.FindProperty("m_casterScope"));
                switch (t.casterScope)
                {
                    case ShadowRaytracer.ObjectScope.SelectedScenes:
                        EditorGUILayout.PropertyField(so.FindProperty("m_casterScenes"), true);
                        break;
                    case ShadowRaytracer.ObjectScope.SelectedObjects:
                        EditorGUILayout.PropertyField(so.FindProperty("m_casterObjects"), true);
                        break;
                }
            }
            else
            {
                EditorGUILayout.PropertyField(so.FindProperty("m_geometryScope"));
                switch (t.geometryScope)
                {
                    case ShadowRaytracer.ObjectScope.SelectedScenes:
                        EditorGUILayout.PropertyField(so.FindProperty("m_geometryScenes"), true);
                        break;
                    case ShadowRaytracer.ObjectScope.SelectedObjects:
                        EditorGUILayout.PropertyField(so.FindProperty("m_geometryObjects"), true);
                        break;
                }
            }

            EditorGUILayout.Space();

            EditorGUILayout.PropertyField(so.FindProperty("m_cullBackFace"));
            EditorGUILayout.PropertyField(so.FindProperty("m_GPUSkinning"));

            so.ApplyModifiedProperties();
        }
    }
}
