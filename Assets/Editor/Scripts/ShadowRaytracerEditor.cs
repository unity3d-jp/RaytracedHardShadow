using UnityEngine;
using UnityEditor;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    [CustomEditor(typeof(UTJ.RaytracedHardShadow.ShadowRaytracer))]
    public class ShadowRaytracerEditor : Editor
    {
        static readonly int indentSize = 16;

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
                int layerToRemove = -1;

                var spLayers = so.FindProperty("m_layers");
                int layerCount = spLayers.arraySize;
                for (int li = 0; li < layerCount; ++li)
                {
                    var spLayer = spLayers.GetArrayElementAtIndex(li);
                    var fold = spLayer.FindPropertyRelative("fold");

                    GUILayout.BeginHorizontal();
                    GUILayout.BeginVertical("Box");
                    GUILayout.BeginHorizontal();
                    GUILayout.Space(indentSize);
                    fold.boolValue = EditorGUILayout.Foldout(fold.boolValue, "Layer " + (li + 1));
                    if (GUILayout.Button("-", GUILayout.Width(20)))
                        layerToRemove = li;
                    GUILayout.EndHorizontal();
                    EditorGUILayout.Space();

                    if (fold.boolValue)
                    {
                        var spReceiverScope = spLayer.FindPropertyRelative("receiverScope");
                        var spCasterScope = spLayer.FindPropertyRelative("casterScope");

                        EditorGUI.indentLevel++;
                        EditorGUILayout.PropertyField(spReceiverScope);
                        EditorGUI.indentLevel++;
                        switch ((ShadowRaytracer.ObjectScope)spReceiverScope.intValue)
                        {
                            case ShadowRaytracer.ObjectScope.SelectedScenes:
                                EditorGUILayout.PropertyField(spLayer.FindPropertyRelative("receiverScenes"), true);
                                break;
                            case ShadowRaytracer.ObjectScope.SelectedObjects:
                                EditorGUILayout.PropertyField(spLayer.FindPropertyRelative("receiverObjects"), true);
                                break;
                        }
                        EditorGUI.indentLevel--;
                        EditorGUILayout.Space();

                        EditorGUILayout.PropertyField(spCasterScope);
                        EditorGUI.indentLevel++;
                        switch ((ShadowRaytracer.ObjectScope)spCasterScope.intValue)
                        {
                            case ShadowRaytracer.ObjectScope.SelectedScenes:
                                EditorGUILayout.PropertyField(spLayer.FindPropertyRelative("casterScenes"), true);
                                break;
                            case ShadowRaytracer.ObjectScope.SelectedObjects:
                                EditorGUILayout.PropertyField(spLayer.FindPropertyRelative("casterObjects"), true);
                                break;
                        }
                        EditorGUI.indentLevel--;
                        EditorGUI.indentLevel--;
                    }

                    GUILayout.EndVertical();
                    GUILayout.EndHorizontal();

                }
                if (layerToRemove != -1)
                {
                    spLayers.DeleteArrayElementAtIndex(layerToRemove);
                }

                if (layerCount < ShadowRaytracer.kMaxLayers)
                {
                    GUILayout.BeginHorizontal();
                    GUILayout.BeginHorizontal("Box");
                    GUILayout.Space(indentSize);
                    GUILayout.FlexibleSpace();
                    if (GUILayout.Button("+", GUILayout.Width(20)))
                    {
                        Undo.RecordObject(t, "ShadowRaytracer");
                        t.AddLayer();
                    }
                    GUILayout.EndHorizontal();
                    GUILayout.EndHorizontal();
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
