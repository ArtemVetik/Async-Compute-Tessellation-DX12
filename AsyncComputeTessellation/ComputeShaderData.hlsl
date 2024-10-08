#ifndef COMPUTE_SHADER_DATA
#define COMPUTE_SHADER_DATA

#include "Structs.hlsl"

groupshared float cam_height_local;

RWStructuredBuffer<Vertex> MeshDataVertex : register(u0);
RWStructuredBuffer<uint> MeshDataIndex : register(u1);
RWStructuredBuffer<uint> DrawArgs : register(u2);
RWStructuredBuffer<uint4> SubdBufferIn : register(u3);
RWStructuredBuffer<uint4> SubdBufferOut : register(u4);
RWStructuredBuffer<uint4> SubdBufferOutCulled : register(u5);
RWStructuredBuffer<uint> SubdCounter : register(u6);

#endif