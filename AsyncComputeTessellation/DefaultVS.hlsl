#define COMPUTE_SHADER 0

#include "Noise.hlsl"
#include "Common.hlsl"

VertexOut main(VertexIn vIn, uint instanceID : SV_InstanceID)
{
    VertexOut output;
    
    float2 leaf_pos = vIn.PosL.xy;
    uint4 key = SubdBufferOut[instanceID];
    uint2 nodeID = key.xy;

    Triangle t;
    ts_getMeshTriangle(key.z, t);
    
    float2 tree_pos = ts_Leaf_to_Tree_64(leaf_pos, nodeID);
    float3 vertex = ts_mapTo3DTriangle(t, tree_pos);
    
    float4 posW = mul(float4(vertex, 1.0f), world);
    
#if USE_DISPLACE
    posW = float4(displaceVertex(posW.xyz, camPosition), 1);
#endif
    
    output.PosW = posW;
    output.NormalW = 1;
    output.PosH = mul(mul(posW, view), projection);
    output.TexC = 1;
	
    return output;
}


