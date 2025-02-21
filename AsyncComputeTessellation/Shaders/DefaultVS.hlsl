#define COMPUTE_SHADER 0

#include "ConstantBuffers.hlsl"
#include "DefaultShaderData.hlsl"

VertexOut main(VertexIn vIn, uint vertexId : SV_VertexID)
{
    VertexOut output;
    
#if SHADOW_MAP
    output.PosH = mul(float4(vIn.PosW, 1), gShadowViewProj);
#else
    output.PosH = mul(float4(vIn.PosW, 1), gViewProj);
#endif
    output.PosW = vIn.PosW;
    output.NormalW = vIn.NormalW;
    output.TexC = vIn.TexC;
    output.LeafPos = vIn.LeafPos;
    output.Lvl = vIn.Lvl;
    
    return output;
}


