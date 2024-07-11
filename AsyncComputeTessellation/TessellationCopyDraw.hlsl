
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

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    //DrawArgs[0] = 0; // Virtual address of VB at slot 3 (64-bit)
    //DrawArgs[1] = 0; // Virtual address of VB at slot 3 (64-bit)
    //DrawArgs[2] = 0; // VB size
    //DrawArgs[3] = 0; // VB stride
    //DrawArgs[4] = 3; // vertexCountPerInstance (or index count if using an index buffer)
    DrawArgs[5] = SubdCounter[1]; // instanceCount
    //DrawArgs[6] = 0; // StartVertexLocation
    //DrawArgs[7] = 0; // StartInstanceLocation
    
    SubdCounter[0] = SubdCounter[1];
    SubdCounter[1] = 0;
}