using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

#if UNITY_EDITOR
public class DebugCommands
{
    [MenuItem("Debug/Print Layers")]
    public static void MakePackage()
    {
        var sb = new System.Text.StringBuilder();
        foreach (var go in Selection.gameObjects)
            sb.AppendFormat("{0}: {1}\n", go.name, go.layer);
        Debug.Log(sb);
    }


    [MenuItem("Debug/Create Dynamic Mesh")]
    public static void CreateDynamicMesh()
    {
        CreateMesh(true);
    }

    [MenuItem("Debug/Create Static Mesh")]
    public static void CreateStaticMesh()
    {
        CreateMesh(false);
    }

    public static void CreateMesh(bool dynamic)
    {
        var mesh = new Mesh();
        mesh.name = dynamic ? "Dynamic Mesh" : "Static Mesh";
        mesh.SetVertices(new List<Vector3> {
            new Vector3(0, 0, 0),
            new Vector3(1, 0, 0),
            new Vector3(0, 1, 0),
        });
        mesh.subMeshCount = 1;
        mesh.SetIndices(new int[] { 0, 1, 2 }, MeshTopology.Triangles, 0);
        if (dynamic)
            mesh.MarkDynamic();
        mesh.UploadMeshData(false);

        var go = new GameObject();
        go.name = mesh.name;
        var mf = go.AddComponent<MeshFilter>();
        var mr = go.AddComponent<MeshRenderer>();
        mf.sharedMesh = mesh;
        mr.material = new Material(Shader.Find("Standard"));

        Selection.activeGameObject = go;
    }

    [MenuItem("Debug/Move Vertices")]
    public static void MoveVertices()
    {
        var go = Selection.activeGameObject;
        if (go == null)
            return;

        var mf = go.GetComponent<MeshFilter>();
        if (mf == null)
            return;
        var mesh = mf.sharedMesh;
        if (mesh == null)
            return;

        var moveAmount = new Vector3(0.25f, 0.0f, 0.0f);
        var vertices = new List<Vector3>();
        mesh.GetVertices(vertices);
        var n = vertices.Count;
        for (int vi = 0; vi < n; ++vi)
        {
            var v = vertices[vi];
            v += moveAmount;
            vertices[vi] = v;
        }
        mesh.SetVertices(vertices);
        mesh.UploadMeshData(false);

        var vb = (ulong)mesh.GetNativeVertexBufferPtr(0);
        var ib = (ulong)mesh.GetNativeIndexBufferPtr();
        Debug.Log(string.Format("MoveVertices(): vb={0:X} ib={1:X}", vb, ib));
    }
}
#endif
