using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace UTJ.RaytracedHardShadow
{
    public enum ShadowCasterLightType
    {
        Directional,
        Point,
        ReversePoint,
    }

    [ExecuteInEditMode]
    public class ShadowCasterLight : MonoBehaviour
    {
#if UNITY_EDITOR
        [MenuItem("GameObject/RaytracedHardShadow/Create Shadow Caster Light", false, 10)]
        public static void CreateMeshSyncServer(MenuCommand menuCommand)
        {
            var go = new GameObject();
            go.name = "Shadow Caster Light";
            var srt = go.AddComponent<ShadowCasterLight>();
            Undo.RegisterCreatedObjectUndo(go, "ShadowRaytracer");
        }
#endif

        [SerializeField] ShadowCasterLightType m_lightType;

        public ShadowCasterLightType lightType
        {
            get { return m_lightType; }
            set { m_lightType = value; }
        }
    }
}
