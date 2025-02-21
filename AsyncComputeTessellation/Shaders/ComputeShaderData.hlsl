#ifndef COMPUTE_SHADER_DATA
#define COMPUTE_SHADER_DATA

#include "Structs.hlsl"

struct SPrepassVertexOut
{
    float3 PosW;
    uint Lvl;
    float3 NormalW;
    uint Padding0;
    float2 TexC;
    float2 LeafPos;
};

groupshared float cam_height_local;

RWStructuredBuffer<uint4> SubdBufferIn : register(u0);
RWStructuredBuffer<uint4> SubdBufferOut : register(u1);
RWStructuredBuffer<uint4> SubdBufferOutCulled : register(u2);
RWStructuredBuffer<SPrepassVertexOut> PrepassVertexOut : register(u3);
RWStructuredBuffer<uint> PrepassIndexOut : register(u4);
RWStructuredBuffer<uint> SubdCounter : register(u5);
RWStructuredBuffer<uint> DrawArgs : register(u6);

StructuredBuffer<Vertex> MeshDataVertex : register(t0);
StructuredBuffer<uint> MeshDataIndex : register(t1);
StructuredBuffer<float2> LeafVertex : register(t2);
StructuredBuffer<uint> LeafIndex : register(t3);

#endif