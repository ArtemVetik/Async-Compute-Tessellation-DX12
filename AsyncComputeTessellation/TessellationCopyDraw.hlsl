
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

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    //DrawArgs[0] = 0; // Virtual address of VB at slot 3 (64-bit)
    //DrawArgs[1] = 0; // Virtual address of VB at slot 3 (64-bit)
    //DrawArgs[2] = 0; // VB size
    //DrawArgs[3] = 0; // VB stride
    //DrawArgs[4] = 3; // vertexCountPerInstance (or index count if using an index buffer)
    DrawArgs[5] = SubdBuffer.IncrementCounter(); // instanceCount
    //DrawArgs[6] = 0; // StartVertexLocation
    //DrawArgs[7] = 0; // StartInstanceLocation
}