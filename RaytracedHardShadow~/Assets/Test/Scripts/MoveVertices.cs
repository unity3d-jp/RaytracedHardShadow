using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class MoveVertices : MonoBehaviour
{
    public Vector3 m_moveValue;

    void Update()
    {
        var mf = GetComponent<MeshFilter>();
        if (mf == null)
            return;
        var mesh = mf.sharedMesh;
        if (mesh == null)
            return;

        var vertices = new List<Vector3>();
        mesh.GetVertices(vertices);
        var n = vertices.Count;
        for (int vi= 0; vi < n; ++vi)
        {
            var v = vertices[vi];
            v += m_moveValue;
            vertices[vi] = v;
        }
        mesh.SetVertices(vertices);
        mesh.UploadMeshData(false);
    }
}
