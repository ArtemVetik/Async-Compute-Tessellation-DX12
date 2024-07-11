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

StructuredBuffer<float3> MeshData : register(t0);
StructuredBuffer<uint4> SubdBuffer : register(t1);

VertexOut main(VertexIn vIn, uint instanceID : SV_InstanceID)
{
    VertexOut output;
    
    float2 leaf_pos = vIn.PosL.xy;
    uint4 key = SubdBuffer[instanceID];
    
    float3 v1 = MeshData.Load(key.z * 3 + 0);
    float3 v2 = MeshData.Load(key.z * 3 + 1);
    float3 v3 = MeshData.Load(key.z * 3 + 2);
    
    uint2 c1 = uint2(1, 0);
    uint2 c2 = uint2(0, 1);
    uint2 c3 = uint2(0, 0);
    uint3x2 xf = uint3x2(c1, c2, c3);
    float2 tree_pos = mul(float3(leaf_pos, 1), xf);
    
    float3 vertex = v1 * (1.0 - tree_pos.x - tree_pos.y) + v3 * tree_pos.x + v2 * tree_pos.y;
    
    float4 posW = mul(float4(vertex, 1.0f), world);
    
    //posW = float4(displaceVertex(posW.xyz, camPosition), 1);
    
    output.PosW = posW;
    output.NormalW = 1;
    output.PosH = mul(mul(posW, view), projection);
    output.TexC = 1;
	
    return output;
}


