using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

#if UNITY_EDITOR
public class TestMenu
{
    [MenuItem("Debug/Print Layers")]
    public static void MakePackage()
    {
        var sb = new System.Text.StringBuilder();
        foreach (var go in Selection.gameObjects)
            sb.AppendFormat("{0}: {1}\n", go.name, go.layer);
        Debug.Log(sb);
    }
}
#endif
