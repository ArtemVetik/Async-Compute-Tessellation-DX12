
cbuffer tessellationData : register(b0)
{
    uint subdivisionLevel;
    uint3 padding;
};

RWStructuredBuffer<uint> DrawArgs : register(u0);
RWStructuredBuffer<uint4> SubdBufferIn : register(u1);
RWStructuredBuffer<uint4> SubdBufferOut : register(u2);
RWStructuredBuffer<uint> SubdCounter : register(u3);

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    //DrawArgs[0] = 0; // Virtual address of VB (64-bit)
    //DrawArgs[1] = 0; // Virtual address of VB (64-bit)
    //DrawArgs[2] = 0; // VB size
    //DrawArgs[3] = 0; // VB stride
    //DrawArgs[4] = 0; // Virtual address of IB (64-bit)
    //DrawArgs[5] = 0; // Virtual address of IB (64-bit)
    //DrawArgs[6] = 0; // IB size
    //DrawArgs[7] = 0; // IB format
    //DrawArgs[8] = 0; // IndexCountPerInstance
    DrawArgs[9] = SubdCounter[1]; // InstanceCount
    //DrawArgs[10] = 0; // StartIndexLocation
    //DrawArgs[11] = 0; // BaseVertexLocation
    //DrawArgs[12] = 0; // StartInstanceLocation
    
    SubdCounter[0] = SubdCounter[1];
    SubdCounter[1] = 0;
}