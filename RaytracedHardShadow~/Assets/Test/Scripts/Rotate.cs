using System.Collections;
using System.Collections.Generic;
using UnityEngine;

[ExecuteInEditMode]
public class Rotate : MonoBehaviour
{
    public Vector3 m_rotateValue;

    void Update()
    {
        var trans = transform;
        var rot = trans.eulerAngles;
        rot += m_rotateValue;
        trans.eulerAngles = rot;
    }
}
