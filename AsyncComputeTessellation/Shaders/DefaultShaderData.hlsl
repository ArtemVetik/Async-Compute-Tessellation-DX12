#ifndef DEFAULT_SHADER_DATA
#define DEFAULT_SHADER_DATA

#include "Structs.hlsl"

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

Texture2D gDiffuseMap : register(t0);

struct VertexIn
{
    float3 PosW : POSITION0;
    uint Lvl : BLENDINDICES;
    float3 NormalW : NORMAL;
    uint Padding0 : COLOR0;
    float2 TexC : TEXCOORD;
    float2 LeafPos : POSITION1;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    uint Lvl : TEXCOORD1;
    float2 LeafPos : TEXCOORD2;
};

struct ps_output
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

#endif