Texture2D gAlbedoTexture : register(t0);
Texture2D gNormalTexture : register(t1);
Texture2D gAccumTexture : register(t2);
Texture2D gDepthTexture : register(t3);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbMotionBlureBuffer : register(b0)
{
    float4x4 gViewProj;
    float4x4 gPreviousViewProj;
    float4x4 gViewInv;
    float4x4 gProjInv;
    float gBlureAmount;
    uint gSampleCount;
    uint2 padding;
};

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD0;
};

VertexOut VS(VertexIn vIn)
{
    VertexOut vOut;
    vOut.PosH = float4(vIn.PosL, 1.0f);
    vOut.TexC = vIn.TexC;
    return vOut;
}

float4 PS(VertexOut pIn) : SV_TARGET
{
    float z = gDepthTexture.Sample(gsamPointWrap, pIn.TexC).r;
    float4 clipSpacePosition = float4(pIn.TexC * 2 - 1, z, 1);
    clipSpacePosition.y *= -1.0f;
    float4 viewSpacePosition = mul(gProjInv, clipSpacePosition);
    viewSpacePosition /= viewSpacePosition.w;
    float4 worldSpacePosition = mul(gViewInv, viewSpacePosition);
    
    float4 currentPosition = mul(worldSpacePosition, gViewProj);
    float4 prevPosition = mul(worldSpacePosition, gPreviousViewProj);
    
    float2 currentPos = currentPosition.xy / currentPosition.w;
    float2 previousPos = prevPosition.xy / prevPosition.w;
    float2 velocity = currentPos - previousPos;
    
    velocity = clamp(velocity, -0.0004f, 0.0004f) * (1 - z / 2.0f);
    
    float4 color = float4(0.0, 0.0, 0.0, 0.0);
    
    for (int i = 0; i < gSampleCount; ++i)
    {
        float t = i / (float) (gSampleCount - 1);
        float2 sampleUv = pIn.TexC + velocity * (t - 0.5) * gBlureAmount;
        color += gAccumTexture.Sample(gsamLinearClamp, sampleUv);
    }
    color /= gSampleCount;
    
    return color;
}