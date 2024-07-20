#ifndef DEFAULT_SHADER_DATA
#define DEFAULT_SHADER_DATA

StructuredBuffer<float3> MeshDataVertex : register(t0);
StructuredBuffer<uint> MeshDataIndex : register(t1);
StructuredBuffer<uint4> SubdBufferOut : register(t2);

struct VertexIn
{
    float3 PosL : POSITION;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float3 NormalW : NORMAL;
    float2 TexC : TEXCOORD;
    uint Lvl : TEXCOORD1;
    float2 LeafPos : TEXCOORD2;
};

#endif