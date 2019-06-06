#if UNITY_EDITOR
using UnityEngine;
using UnityEditor;


public class RaytracedHardShadowPackaging
{
    [MenuItem("Assets/Make RaytracedHardShadow.unitypackage")]
    public static void MakePackage()
    {
        string[] files = new string[]
        {
            "Assets/UTJ/RaytracedHardShadow",
        };
        AssetDatabase.ExportPackage(files, "RaytracedHardShadow.unitypackage", ExportPackageOptions.Recurse);
    }

}
#endif // UNITY_EDITOR
