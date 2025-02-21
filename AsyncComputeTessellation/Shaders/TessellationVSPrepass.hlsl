#define COMPUTE_SHADER 1

#include "ConstantBuffers.hlsl"
#include "ComputeShaderData.hlsl"
#include "Common.hlsl"
#include "Noise.hlsl"

[numthreads(64, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id >= SubdCounter[2] * gIndicesCount)
        return;
    
    uint instanceID = id / gIndicesCount;
    uint vertexId = id - (instanceID * gIndicesCount);
    
    uint indexId = LeafIndex.Load(vertexId);
    
    FTYPE2 leaf_pos = LeafVertex.Load(indexId);
    
    uint4 key = SubdBufferOutCulled[instanceID];
    uint2 nodeID = key.xy;

    Triangle t;
    ts_getMeshTriangle(key.z, t);
    
    FTYPE2 tree_pos = ts_Leaf_to_Tree_64(leaf_pos, nodeID);
    Vertex vertex = ts_interpolateVertex(t, tree_pos);
    
    float4 posW = mul(vertex.Position, gWorld);
    
#if USE_DISPLACE
    posW = float4(displaceVertex(posW.xyz, gCamPosition), 1);
#endif
    
    SPrepassVertexOut output;
    output.PosW = posW.xyz;
    output.NormalW = mul(vertex.Normal, gWorld).xyz;
    output.TexC = vertex.TexC;
    
    output.Lvl = ts_findMSB_64(key.xy);
    output.LeafPos = 0;
    output.Padding0 = 0;
    
    uint triangleId = gTrianglesCount * instanceID + indexId;
    
    PrepassVertexOut[triangleId] = output;
    PrepassIndexOut[id] = triangleId;
}
