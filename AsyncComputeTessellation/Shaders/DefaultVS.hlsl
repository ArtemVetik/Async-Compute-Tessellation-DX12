#define COMPUTE_SHADER 0

#include "ConstantBuffers.hlsl"
#include "DefaultShaderData.hlsl"
#ifdef USE_STANDART_TESSELLATION
#include "Common.hlsl"
#include "Noise.hlsl"
#endif

VertexOut main(VertexIn vIn
#ifdef USE_STANDART_TESSELLATION
               , uint instanceID : SV_InstanceID
#endif
)
{
#ifdef USE_STANDART_TESSELLATION
    
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
#endif // USE_DISPLACE
    
#if SHADOW_MAP
    output.TexC = vertex.TexC;
    output.PosH = mul(posW, gShadowViewProj);
#else
    output.PosW = posW.xyz;
    output.NormalW = mul(vertex.Normal, gWorld).xyz;
    output.Lvl = ts_findMSB_64(key.xy);
    output.TexC = vertex.TexC;
    output.PosH = mul(posW, gViewProj);
#endif // SHADOW_MAP
    
    return output;
#else
    
    VertexOut output;
#if SHADOW_MAP
    output.PosH = mul(float4(vIn.PosW, 1), gShadowViewProj);
#else
    output.PosH = mul(float4(vIn.PosW, 1), gViewProj);
#endif // SHADOW_MAP
    output.PosW = vIn.PosW;
    output.NormalW = vIn.NormalW;
    output.TexC = vIn.TexC;
    output.LeafPos = vIn.LeafPos;
    output.Lvl = vIn.Lvl;
    
    return output;
    
#endif // USE_STANDART_TESSELLATION
}


