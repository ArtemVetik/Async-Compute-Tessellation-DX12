
cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float aspectRatio;
};

RWStructuredBuffer<float3> MeshData : register(u0);
RWStructuredBuffer<uint> DrawArgs : register(u1);
RWStructuredBuffer<uint4> SubdBuffer : register(u2);

[numthreads(8, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    SubdBuffer.IncrementCounter();
}
