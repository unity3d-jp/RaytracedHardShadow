using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace UTJ.RaytracedHardShadow
{
    public enum ShadowCasterLightType
    {
        Directional,
        Spot,
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
        [SerializeField] float m_range = 10.0f;
        [SerializeField] [Range(1.0f, 179.0f)] float m_spotAngle = 30.0f;

        public ShadowCasterLightType lightType
        {
            get { return m_lightType; }
            set { m_lightType = value; }
        }

        public float range
        {
            get { return m_range; }
            set { m_range = value; }
        }

        public float spotAngle
        {
            get { return m_spotAngle; }
            set { m_spotAngle = value; }
        }
    }
}
