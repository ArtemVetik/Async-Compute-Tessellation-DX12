#include "LightingUtil.hlsl"
Texture2D gAccumTexture : register(t0);
Texture2D gBloomTexture : register(t1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbLightPass : register(b0)
{
    float4x4 gViewInv;
    float4x4 gProjInv;
    float4 gDiffuseAlbedo;
    float4 gAmbientLight;
    float3 gEyePosW;
    float gRoughness;
    float3 gFresnelR0;
    uint space0;
    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut vOut;
    vOut.PosH = float4(vIn.PosL, 1.0f);
    vOut.TexC = vIn.TexC;
    return vOut;
}

[earlydepthstencil]
float4 PS(VertexOut pIn) : SV_TARGET
{
    float4 hdrColor = gAccumTexture.Sample(gsamPointWrap, pIn.TexC);
    float4 bloomColor = gBloomTexture.Sample(gsamPointWrap, pIn.TexC);
    
    hdrColor += 1.0f * bloomColor;
    
    float gamma = 2.2;
  
    // reinhard tone mapping
    float3 mapped = hdrColor / (hdrColor + (1.0f));
    mapped = pow(mapped, (1.0 / gamma));
  
    return float4(mapped, 1.0);
}