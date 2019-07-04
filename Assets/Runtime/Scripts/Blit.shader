Shader "Hidden/UTJ/RaytracedHardShadow/Blit"
{
    Properties
    {
        _MainTex ("Texture", 2D) = "white" {}
    }
    SubShader
    {
        Cull Off
        ZWrite Off
        ZTest Always

        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment frag

            #include "UnityCG.cginc"

            sampler2D _MainTex;

            float4 frag(v2f_img i) : SV_Target
            {
                float4 ret;
                ret.rgb = tex2D(_MainTex, i.uv).r;
                ret.a = 1.0;
                return ret;
            }
            ENDCG
        }
    }
}
