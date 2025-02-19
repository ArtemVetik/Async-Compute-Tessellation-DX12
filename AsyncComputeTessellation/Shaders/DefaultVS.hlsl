#define COMPUTE_SHADER 0

#include "ConstantBuffers.hlsl"
#include "Noise.hlsl"
#include "Common.hlsl"

VertexOut main(VertexIn vIn, uint instanceID : SV_InstanceID)
{
    VertexOut output;
    
    FTYPE2 leaf_pos = vIn.PosL.xy;
    uint4 key = SubdBufferOut[instanceID];
    uint2 nodeID = key.xy;

    Triangle t;
    ts_getMeshTriangle(key.z, t);
    
    FTYPE2 tree_pos = ts_Leaf_to_Tree_64(leaf_pos, nodeID);
    Vertex vertex = ts_interpolateVertex(t, tree_pos);
    
    float4 posW = mul(vertex.Position, gWorld);
    
#if USE_DISPLACE
#if SHADOW_MAP
    posW = float4(displaceVertex(posW.xyz, gLightPos), 1);
#else
    posW = float4(displaceVertex(posW.xyz, gCamPosition), 1);
#endif
#endif
    
#if SHADOW_MAP
    output.TexC = vertex.TexC;
    output.PosH = mul(posW, gShadowViewProj);
#else
    output.PosW = posW.xyz;
    output.NormalW = mul(vertex.Normal, gWorld).xyz;
    output.Lvl = ts_findMSB_64(key.xy);
    output.TexC = vertex.TexC;
    output.PosH = mul(posW, gViewProj);
#endif
    
    return output;
}


