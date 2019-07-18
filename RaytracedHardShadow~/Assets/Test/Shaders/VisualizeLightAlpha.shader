Shader "UTJ/Raytraced Hard Shadow/Visualize Light Alpha"
{
    Properties
    {
        _Multiplyer("Multiplayer", Float) = 1.0
    }
    SubShader
    {
        Tags { "RenderType"="Opaque" }

        Pass
        {
            Tags {"LightMode" = "ForwardBase"}

            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile DIRECTIONAL POINT SPOT

            #include "UnityCG.cginc"
            #include "Lighting.cginc"
            #include "AutoLight.cginc"
            float _Multiplyer;

            struct ia_out
            {
                float4 vertex : POSITION;
            };
            struct vs_out
            {
                float4 vertex : SV_POSITION;
                float4 wpos : TEXCOORD0;
            };

            vs_out vert(ia_out v)
            {
                vs_out o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.wpos = mul(unity_ObjectToWorld, v.vertex);
                return o;
            }

            // light alpha as *red*
            float4 frag(vs_out i) : SV_Target
            {
                UNITY_LIGHT_ATTENUATION(attenuation, 0, i.wpos.xyz);
                return float4(_LightColor0.a * attenuation * _Multiplyer, 0.0f, 0.0f, 1.0f);
            }
            ENDCG
        }

        Pass
        {
            Tags {"LightMode" = "ForwardAdd"}
            Blend One One

            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #pragma multi_compile DIRECTIONAL POINT SPOT

            #include "UnityCG.cginc"
            #include "Lighting.cginc"
            #include "AutoLight.cginc"
            float _Multiplyer;

            struct ia_out
            {
                float4 vertex : POSITION;
            };
            struct vs_out
            {
                float4 vertex : SV_POSITION;
                float4 wpos : TEXCOORD0;
            };

            vs_out vert(ia_out v)
            {
                vs_out o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.wpos = mul(unity_ObjectToWorld, v.vertex);
                return o;
            }

            // light alpha as *green*
            float4 frag(vs_out i) : SV_Target
            {
                UNITY_LIGHT_ATTENUATION(attenuation, 0, i.wpos.xyz);
                return float4(0.0f, _LightColor0.a * attenuation * _Multiplyer, 0.0f, 1.0f);
            }
            ENDCG
        }
    }
}
