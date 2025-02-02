#ifndef COMPUTE_SHADER_DATA
#define COMPUTE_SHADER_DATA

#include "Structs.hlsl"

groupshared float cam_height_local;

RWStructuredBuffer<uint4> SubdBufferIn : register(u0);
RWStructuredBuffer<uint4> SubdBufferOut : register(u1);
RWStructuredBuffer<uint4> SubdBufferOutCulled : register(u2);
RWStructuredBuffer<Vertex> MeshDataVertex : register(u3);
RWStructuredBuffer<uint> MeshDataIndex : register(u4);
RWStructuredBuffer<uint> SubdCounter : register(u5);
RWStructuredBuffer<uint> DrawArgs : register(u6);

#endif