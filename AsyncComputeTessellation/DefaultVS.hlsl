#include "Noise.hlsl"

cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float3 camPosition;
    float aspectRatio;
};

struct VertexIn
{
	float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

StructuredBuffer<float3> VertexPool : register(t0);
StructuredBuffer<uint> DrawList : register(t1);

VertexOut main(uint id : SV_VertexID)
{
    VertexOut output;

    uint drawIndex = DrawList.Load(id);
    float3 vertex = VertexPool.Load(drawIndex);

    float4 posW = mul(float4(vertex, 1.0f), world);
    
    posW = float4(displaceVertex(posW.xyz, camPosition), 1);
    
    output.PosW = posW;
    output.NormalW = 1;
    output.PosH = mul(mul(posW, view), projection);
    output.TexC = 1;
	
    return output;
}


