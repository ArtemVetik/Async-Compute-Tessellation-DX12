Texture2D gAccumTexture : register(t0);
Texture2D gDepthTexture : register(t1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbMotionBlurBuffer : register(b0)
{
    float4x4 gViewProjInv;
    float4x4 gPreviousViewProj;
};

cbuffer cbMotionBlurBuffer : register(b1)
{
    float gBlurAmount;
    uint3 padding;
}

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
    float zOverW = gDepthTexture.Sample(gsamPointWrap, pIn.TexC).r;
    
    if (zOverW >= 1.0f)
        return gAccumTexture.Sample(gsamLinearClamp, pIn.TexC);
    
    float4 H = float4(pIn.TexC.x * 2 - 1, (1 - pIn.TexC.y) * 2 - 1, zOverW, 1);
    float4 worldPos = mul(H, gViewProjInv);
    worldPos /= worldPos.w;
    
    float4 currentPos = H;
    float4 previousPos = mul(worldPos, gPreviousViewProj);
    previousPos /= previousPos.w;
    
    float2 velocity = (currentPos.xy - previousPos.xy) / 2.f;
    
    float4 color = gAccumTexture.Sample(gsamLinearClamp, pIn.TexC);
    pIn.TexC += velocity * gBlurAmount;
    
#if SAMPLE_COUNT
    [unroll]
    for (int i = 1; i < SAMPLE_COUNT; ++i, pIn.TexC += velocity * gBlurAmount)
    {
        color += gAccumTexture.Sample(gsamLinearClamp, pIn.TexC);
    }
    color /= SAMPLE_COUNT;
#endif
    
    return color;
}