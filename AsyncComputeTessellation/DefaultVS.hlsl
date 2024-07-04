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

VertexOut main(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    
    posW = float4(displaceVertex(posW.xyz, camPosition), 1);
    
    vout.PosW = posW;
    vout.NormalW = mul(vin.NormalL, (float3x3) world);
    vout.PosH = mul(mul(posW, view), projection);
	vout.TexC = vin.TexC;
	
    return vout;
}


