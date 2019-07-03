using System.Collections.Generic;
using UnityEngine;
using UnityEditor;
using UTJ.RaytracedHardShadow;

namespace UTJ.RaytracedHardShadowEditor
{
    [CustomEditor(typeof(UTJ.RaytracedHardShadow.ShadowRaytracer))]
    public class ShadowRaytracerEditor : Editor
    {
        // return drag & dropped scenes from *Hierarchy*.
        // scene assets from Project are ignored. these should be handled by Unity's default behavior.
        public static SceneAsset[] GetDroppedScenesFromHierarchy(Rect dropArea)
        {
            List<SceneAsset> ret = null;

            var evt = Event.current;
            if (evt.type == EventType.DragUpdated || evt.type == EventType.DragPerform)
            {
                if (dropArea.Contains(evt.mousePosition))
                {
                    DragAndDrop.visualMode = DragAndDropVisualMode.Copy;

                    if (evt.type == EventType.DragPerform)
                    {
                        // scene assets from Project have both objectReferences and paths.
                        // scenes from Hierarchy have only paths.
                        if (DragAndDrop.objectReferences.Length == 0 && DragAndDrop.paths.Length != 0)
                        {
                            DragAndDrop.AcceptDrag();

                            ret = new List<SceneAsset>();
                            foreach (var path in DragAndDrop.paths)
                            {
                                if (path.EndsWith(".unity"))
                                {
                                    var sceneAsset = AssetDatabase.LoadAssetAtPath<SceneAsset>(path);
                                    if (sceneAsset != null)
                                        ret.Add(sceneAsset);
                                }
                            }
                        }
                    }
                }
            }

            if (ret != null && ret.Count > 0)
            {
                evt.Use();
                return ret.ToArray();
            }
            return null;
        }

        public static bool AddDroppedScenesFromHierarchy(SerializedProperty dst, Rect dropArea)
        {
            var sceneAssets = GetDroppedScenesFromHierarchy(dropArea);
            if (sceneAssets != null)
            {
                foreach (var s in sceneAssets)
                {
                    int i = dst.arraySize;
                    dst.InsertArrayElementAtIndex(i);
                    dst.GetArrayElementAtIndex(i).objectReferenceValue = s;
                }
                return true;
            }
            return false;
        }

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

            {
                var spLightScope = so.FindProperty("m_lightScope");
                EditorGUILayout.PropertyField(spLightScope);
                switch ((ShadowRaytracer.ObjectScope)spLightScope.intValue)
                {
                    case ShadowRaytracer.ObjectScope.Scenes:
                        {
                            var spScenes = so.FindProperty("m_lightScenes");
                            EditorGUILayout.PropertyField(spScenes, true);
                            AddDroppedScenesFromHierarchy(spScenes, GUILayoutUtility.GetLastRect());
                        }
                        break;
                    case ShadowRaytracer.ObjectScope.Objects:
                        EditorGUILayout.PropertyField(so.FindProperty("m_lightObjects"), true);
                        break;
                }
            }
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
                            case ShadowRaytracer.ObjectScope.Scenes:
                                {
                                    var spScenes = spLayer.FindPropertyRelative("receiverScenes");
                                    EditorGUILayout.PropertyField(spScenes, true);
                                    AddDroppedScenesFromHierarchy(spScenes, GUILayoutUtility.GetLastRect());
                                }
                                break;
                            case ShadowRaytracer.ObjectScope.Objects:
                                EditorGUILayout.PropertyField(spLayer.FindPropertyRelative("receiverObjects"), true);
                                break;
                        }
                        EditorGUI.indentLevel--;
                        EditorGUILayout.Space();

                        EditorGUILayout.PropertyField(spCasterScope);
                        EditorGUI.indentLevel++;
                        switch ((ShadowRaytracer.ObjectScope)spCasterScope.intValue)
                        {
                            case ShadowRaytracer.ObjectScope.Scenes:
                                {
                                    var spScenes = spLayer.FindPropertyRelative("casterScenes");
                                    EditorGUILayout.PropertyField(spScenes, true);
                                    AddDroppedScenesFromHierarchy(spScenes, GUILayoutUtility.GetLastRect());
                                }
                                break;
                            case ShadowRaytracer.ObjectScope.Objects:
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
                    spLayers.DeleteArrayElementAtIndex(layerToRemove);

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
                    case ShadowRaytracer.ObjectScope.Scenes:
                        {
                            var spScenes = so.FindProperty("m_geometryScenes");
                            EditorGUILayout.PropertyField(spScenes, true);
                            AddDroppedScenesFromHierarchy(spScenes, GUILayoutUtility.GetLastRect());
                        }
                        break;
                    case ShadowRaytracer.ObjectScope.Objects:
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
