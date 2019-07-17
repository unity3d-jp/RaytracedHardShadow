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

        // spScenes: SceneAsset[]
        public static void DrawScenesField(SerializedProperty spScenes)
        {
            EditorGUILayout.PropertyField(spScenes, true);

            var sceneAssets = GetDroppedScenesFromHierarchy(GUILayoutUtility.GetLastRect());
            if (sceneAssets != null)
            {
                foreach (var s in sceneAssets)
                {
                    int i = spScenes.arraySize;
                    spScenes.InsertArrayElementAtIndex(i);
                    spScenes.GetArrayElementAtIndex(i).objectReferenceValue = s;
                }
            }
        }


        //static readonly int indentSize = 16;
        static List<ShadowRaytracerEditor> s_instances;

        void OnEnable()
        {
            if (s_instances == null)
                s_instances = new List<ShadowRaytracerEditor>();
            s_instances.Add(this);
            if (s_instances.Count == 1)
            {
#if UNITY_2019_1_OR_NEWER
                SceneView.duringSceneGui += OnSceneGUI;
#else
                SceneView.onSceneGUIDelegate += OnSceneGUI;
#endif
            }
        }

        void OnDisable()
        {
            s_instances.Remove(this);
            if (s_instances.Count == 0)
            {
#if UNITY_2019_1_OR_NEWER
                SceneView.duringSceneGui -= OnSceneGUI;
#else
                SceneView.onSceneGUIDelegate -= OnSceneGUI;
#endif
            }
        }

        static void OnSceneGUI(SceneView sceneView)
        {
            bool timestamp = (rthsGlobals.debugFlags & rthsDebugFlag.Timestamp) != 0;
            if (timestamp)
            {
                foreach (var inst in s_instances)
                {
                    var t = inst.target as ShadowRaytracer;
                    if (t != null)
                        inst.Repaint();
                }
            }
        }

        public override void OnInspectorGUI()
        {
            //DrawDefaultInspector();

            var t = target as ShadowRaytracer;
            var so = serializedObject;

            // output
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Output", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(so.FindProperty("m_generateRenderTexture"));
            {
                EditorGUI.indentLevel++;
                if (t.generateRenderTexture)
                    EditorGUI.BeginDisabledGroup(true);
                EditorGUILayout.PropertyField(so.FindProperty("m_outputTexture"));
                if (t.generateRenderTexture)
                    EditorGUI.EndDisabledGroup();
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

            // shadow
            EditorGUILayout.LabelField("Shadow", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(so.FindProperty("m_cullBackFaces"));
            if (t.cullBackFaces)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.PropertyField(so.FindProperty("m_flipCasterFaces"));
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.PropertyField(so.FindProperty("m_ignoreSelfShadow"));
            if (t.ignoreSelfShadow)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.PropertyField(so.FindProperty("m_keepSelfDropShadow"));
                EditorGUILayout.PropertyField(so.FindProperty("m_selfShadowThreshold"));
                EditorGUI.indentLevel--;
            }
            else
            {
                EditorGUILayout.PropertyField(so.FindProperty("m_shadowRayOffset"));
            }
            EditorGUILayout.Space();

            // lights
            EditorGUILayout.LabelField("Lights", EditorStyles.boldLabel);
            {
                var spLightScope = so.FindProperty("m_lightScope");
                EditorGUILayout.PropertyField(spLightScope);
                switch ((ShadowRaytracer.ObjectScope)spLightScope.intValue)
                {
                    case ShadowRaytracer.ObjectScope.Scenes:
                        DrawScenesField(so.FindProperty("m_lightScenes"));
                        break;
                    case ShadowRaytracer.ObjectScope.Objects:
                        EditorGUILayout.PropertyField(so.FindProperty("m_lightObjects"), true);
                        break;
                }
            }
            EditorGUILayout.Space();

            // geometry
            EditorGUILayout.LabelField("Geometry", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(so.FindProperty("m_geometryScope"));
            switch (t.geometryScope)
            {
                case ShadowRaytracer.ObjectScope.Scenes:
                    DrawScenesField(so.FindProperty("m_geometryScenes"));
                    break;
                case ShadowRaytracer.ObjectScope.Objects:
                    EditorGUILayout.PropertyField(so.FindProperty("m_geometryObjects"), true);
                    break;
            }
            EditorGUILayout.Space();

            // misc
            EditorGUILayout.LabelField("Misc", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(so.FindProperty("m_GPUSkinning"));
            EditorGUILayout.PropertyField(so.FindProperty("m_adaptiveSampling"));
            EditorGUILayout.PropertyField(so.FindProperty("m_antialiasing"));
            //EditorGUILayout.PropertyField(so.FindProperty("m_parallelCommandList"));

            // debug
            var foldDebug = so.FindProperty("m_foldDebug");
            foldDebug.boolValue = EditorGUILayout.Foldout(foldDebug.boolValue, "Debug");
            if(foldDebug.boolValue)
            {
                var debugFlags = rthsGlobals.debugFlags;
                bool timestamp          = (debugFlags & rthsDebugFlag.Timestamp) != 0;
                bool noLayerCompaction  = (debugFlags & rthsDebugFlag.NoLayerCompaction) != 0;
                bool forceUpdateAS      = (debugFlags & rthsDebugFlag.ForceUpdateAS) != 0;
                bool powerStableState   = (debugFlags & rthsDebugFlag.PowerStableState) != 0;

                EditorGUI.indentLevel++;
                EditorGUI.BeginChangeCheck();
                timestamp           = EditorGUILayout.Toggle("Timestamp", timestamp);
                noLayerCompaction   = EditorGUILayout.Toggle("No Layer Compaction", noLayerCompaction);
                forceUpdateAS       = EditorGUILayout.Toggle("Force Update AS", forceUpdateAS);
                powerStableState    = EditorGUILayout.Toggle("Power Stable State", powerStableState);
                if (EditorGUI.EndChangeCheck())
                {
                    rthsDebugFlag v = 0;
                    if (timestamp)
                        v |= rthsDebugFlag.Timestamp;
                    if (noLayerCompaction)
                        v |= rthsDebugFlag.NoLayerCompaction;
                    if (forceUpdateAS)
                        v |= rthsDebugFlag.ForceUpdateAS;
                    if (powerStableState)
                        v |= rthsDebugFlag.PowerStableState;
                    rthsGlobals.debugFlags = v;
                }

                EditorGUILayout.PropertyField(so.FindProperty("m_dbgVerboseLog"), new GUIContent("Verbose Log"));

                if (timestamp)
                    EditorGUILayout.TextArea(t.timestampLog, GUILayout.Height(80));
                EditorGUI.indentLevel--;
            }

            so.ApplyModifiedProperties();

            EditorGUILayout.Space();
            if (GUILayout.Button("Export to image", GUILayout.Width(200)))
                ExportToImageWindow.Open(t);

            EditorGUILayout.Space();
            EditorGUILayout.LabelField(System.String.Format("Plugin version: {0} ({1})", rthsGlobals.version, rthsGlobals.releaseDate));
        }
    }
}
