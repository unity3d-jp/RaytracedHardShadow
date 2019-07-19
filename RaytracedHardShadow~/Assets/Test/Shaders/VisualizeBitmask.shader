Shader "UTJ/Raytraced Hard Shadow/Visualize Bitmask"
{
    Properties
    {
        _MainTex("Texture", 2D) = "white" {}
        _Intensity("Intensity", Float) = 0.25
    }
    SubShader
    {
        Tags { "RenderType" = "Opaque" }

        Pass
        {
            CGPROGRAM
            #pragma only_renderers d3d11
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

            Texture2D<uint4> _MainTex;
            float4 _MainTex_TexelSize;
            float _Intensity;

            vs_out vert(ia_out v)
            {
                vs_out o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = v.uv;
                return o;
            }

            float4 frag(vs_out i) : SV_Target
            {
                float2 uv = (1.0f.xx - i.uv) * _MainTex_TexelSize.zw;
                uint v = _MainTex[uv].x;

                float r = 0.0f;
                for (int i = 0; i < 32; ++i) {
                    if ((v & (1 << i)) != 0)
                        r += _Intensity;
                }
                return float4(r, r, r, 1.0f);
            }
            ENDCG
        }
    }
}
