#ifndef COMPUTE_SHADER_DATA
#define COMPUTE_SHADER_DATA

groupshared float cam_height_local;

cbuffer tessellationData : register(b0)
{
    matrix meshWorld;
    uint subdivisionLevel;
    float3 camPosition;
    uint screenRes;
    uint3 padding;
};

RWStructuredBuffer<float3> MeshDataVertex : register(u0);
RWStructuredBuffer<uint> MeshDataIndex : register(u1);
RWStructuredBuffer<uint> DrawArgs : register(u2);
RWStructuredBuffer<uint4> SubdBufferIn : register(u3);
RWStructuredBuffer<uint4> SubdBufferOut : register(u4);
RWStructuredBuffer<uint> SubdCounter : register(u5);

#endif