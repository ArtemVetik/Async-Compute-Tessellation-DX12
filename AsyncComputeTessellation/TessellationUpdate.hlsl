
cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float aspectRatio;
};

RWStructuredBuffer<float3> MeshData : register(u0);
RWStructuredBuffer<uint> DrawArgs : register(u1);
RWStructuredBuffer<uint4> SubdBufferIn : register(u2);
RWStructuredBuffer<uint4> SubdBufferOut : register(u3);
RWStructuredBuffer<uint> SubdCounter : register(u4);

[numthreads(8, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id.x >= SubdCounter[0])
        return;
    
    SubdBufferOut[id.x] = SubdBufferIn[id.x];
    InterlockedAdd(SubdCounter[1], 1);
}
