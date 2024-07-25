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

StructuredBuffer<Vertex> MeshDataVertex : register(t0);
StructuredBuffer<uint> MeshDataIndex : register(t1);
StructuredBuffer<uint4> SubdBufferOut : register(t2);

Texture2D gShadowMap : register(t3);

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION0;
    float4 ShadowPosH : POSITION1;
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