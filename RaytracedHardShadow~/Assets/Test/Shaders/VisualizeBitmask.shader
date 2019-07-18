Shader "UTJ/Raytraced Hard Shadow/Visualize Bitmask"
{
    Properties
    {
        _MainTex ("Texture", 2D) = "white" {}
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" }

        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"

            struct ia_out
            {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };
            struct vs_out
            {
                float4 vertex : SV_POSITION;
                float2 uv : TEXCOORD0;
            };

            UNITY_DECLARE_TEX2D_NOSAMPLER_UINT(_MainTex);
            float4 _MainTex_TexelSize;

            vs_out vert(ia_out v)
            {
                vs_out o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = v.uv;
                return o;
            }

            float4 frag(vs_out i) : SV_Target
            {
                uint3 v = _MainTex[i.uv * _MainTex_TexelSize.zw].xyz;
                float s = 1.0f / 4;
                float r = 0.0f;
                for (int i = 0; i < 32; ++i) {
                    if ((v.x & (1 << i)) != 0)
                        r += s;
                }
                return float4(r, r, r, 1.0f);
            }
            ENDCG
        }
    }
}
