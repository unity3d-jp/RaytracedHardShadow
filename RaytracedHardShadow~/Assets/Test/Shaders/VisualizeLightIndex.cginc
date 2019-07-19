#include "UnityCG.cginc"
#include "Lighting.cginc"
#include "AutoLight.cginc"

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

float4 frag(vs_out i) : SV_Target
{
    float4 c = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (_LightColor0.a >= 1000.0f) {
        uint light_index = (uint)_LightColor0.a / 1000 - 1;

        UNITY_LIGHT_ATTENUATION(attenuation, 0, i.wpos.xyz);
        float l = dot(_LightColor0.rgb, 1.0f) / 3.0f * attenuation;
        if (light_index % 3 == 0) c.r += l;
        if (light_index % 3 == 1) c.g += l;
        if (light_index % 3 == 2) c.b += l;
    }
    return c;
}
