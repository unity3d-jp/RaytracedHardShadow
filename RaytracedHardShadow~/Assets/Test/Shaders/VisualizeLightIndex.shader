Shader "UTJ/Raytraced Hard Shadow/Visualize Light Index"
{
    Properties
    {
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
            #include "VisualizeLightIndex.cginc"
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
            #include "VisualizeLightIndex.cginc"
            ENDCG
        }
    }
}
