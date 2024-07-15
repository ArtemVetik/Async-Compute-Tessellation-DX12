#include "Common.hlsl"

cbuffer tessellationData : register(b0)
{
    uint subdivisionLevel;
    uint3 padding;
};

RWStructuredBuffer<float3> MeshData : register(u0);
RWStructuredBuffer<uint> DrawArgs : register(u1);
RWStructuredBuffer<uint4> SubdBufferIn : register(u2);
RWStructuredBuffer<uint4> SubdBufferOut : register(u3);
RWStructuredBuffer<uint> SubdCounter : register(u4);

void compute_writeKey(uint2 new_nodeID, uint4 current_key)
{
    uint4 new_key = uint4(new_nodeID, current_key.zw);
    uint idx;
    InterlockedAdd(SubdCounter[1], 1, idx);

    SubdBufferOut[idx] = new_key;
}

[numthreads(32, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id.x >= SubdCounter[0])
        return;
    
    uint4 key = SubdBufferIn[id.x];
    uint2 nodeID = key.xy;
    
    int targetLod = subdivisionLevel;
    int parentLod = subdivisionLevel;
    
    int keyLod = ts_findMSB_64(nodeID);
    
    // update the key accordingly
    if ( /* subdivide ? */keyLod < targetLod && !ts_isLeaf_64(nodeID))
    {
        uint2 children[2];
        ts_children_64(nodeID, children);
        compute_writeKey(children[0], key);
        compute_writeKey(children[1], key);
    }
    else if ( /* keep ? */keyLod < (parentLod + 1))
    {
        compute_writeKey(nodeID, key);
    }
    else /* merge ? */
    {
        if ( /* is root ? */ts_isRoot_64(nodeID))
        {
            compute_writeKey(nodeID, key);
        }
        else if ( /* is zero child ? */ts_isZeroChild_64(nodeID))
        {
            compute_writeKey(ts_parent_64(nodeID), key);
        }
    }
}
