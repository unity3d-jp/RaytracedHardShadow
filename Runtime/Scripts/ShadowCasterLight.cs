using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace Unity.RaytracedHardShadow
{
    internal enum ShadowCasterLightType {
        Directional,
        Spot,
        Point,
        ReversePoint,
    }

    // this component doesn't light objects. only casts shadows. intended mainly for experiments.
    // (ReversePooint light is supported only by this component)
    [ExecuteInEditMode]
    internal class ShadowCasterLight : MonoBehaviour {
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
