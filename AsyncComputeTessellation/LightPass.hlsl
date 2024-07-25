#include "LightingUtil.hlsl"

Texture2D gAlbedoTexture : register(t0);
Texture2D gNormalTexture : register(t1);
Texture2D gAccumTexture : register(t2);
Texture2D gDepthTexture : register(t3);

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
    float4 diffuseAlbedo = gAlbedoTexture.Sample(gsamPointWrap, pIn.TexC) * gDiffuseAlbedo;
    float4 normal = gNormalTexture.Sample(gsamPointWrap, pIn.TexC);
    
    float z = gDepthTexture.Sample(gsamPointWrap, pIn.TexC).r;
    float4 clipSpacePosition = float4(pIn.TexC * 2 - 1, z, 1);
    clipSpacePosition.y *= -1.0f;
    float4 viewSpacePosition = mul(gProjInv, clipSpacePosition);
    viewSpacePosition /= viewSpacePosition.w;
    float4 worldSpacePosition = mul(gViewInv, viewSpacePosition);
	
    float4 posW = worldSpacePosition;
    float3 toEyeW = normalize(gEyePosW - posW.rgb);
    
    const float shininess = 1.0f - gRoughness;
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
	
    float4 directLight = ComputeLighting(gLights, mat, posW.rgb,
        normal.rgb, toEyeW, float3(normal.a, 1.0f, 1.0f));
		
    float4 ambient = gAmbientLight * diffuseAlbedo;
    float4 litColor = ambient + directLight;
    litColor.a = diffuseAlbedo.a;
	
    return litColor;
}