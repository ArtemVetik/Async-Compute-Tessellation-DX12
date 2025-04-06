#ifndef COMPUTE_SHADER_DATA
#define COMPUTE_SHADER_DATA

#include "Structs.hlsl"

#ifndef USE_STANDART_TESSELLATION
struct SPrepassVertexOut
{
    float3 PosW;
    uint Lvl;
    float3 NormalW;
    uint Padding0;
    float2 TexC;
    float2 LeafPos;
};
#endif

groupshared float cam_height_local;

RWStructuredBuffer<uint4> SubdBufferIn : register(u0);
RWStructuredBuffer<uint4> SubdBufferOut : register(u1);
RWStructuredBuffer<uint4> SubdBufferOutCulled : register(u2);

#ifdef USE_STANDART_TESSELLATION
RWStructuredBuffer<uint> SubdCounter : register(u3);
RWStructuredBuffer<uint> DrawArgs : register(u4);
#else
RWStructuredBuffer<SPrepassVertexOut> PrepassVertexOut : register(u3);
RWStructuredBuffer<uint> PrepassIndexOut : register(u4);
RWStructuredBuffer<uint> SubdCounter : register(u5);
RWStructuredBuffer<uint> DrawArgs : register(u6);
#endif

StructuredBuffer<Vertex> MeshDataVertex : register(t0);
StructuredBuffer<uint> MeshDataIndex : register(t1);

#ifndef USE_STANDART_TESSELLATION
StructuredBuffer<float2> LeafVertex : register(t2);
StructuredBuffer<uint> LeafIndex : register(t3);
#endif

#endif