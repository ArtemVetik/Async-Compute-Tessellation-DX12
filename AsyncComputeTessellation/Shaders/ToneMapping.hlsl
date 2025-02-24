Texture2D gAccumTexture : register(t0);
Texture2D gBloomTexture : register(t1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer AberrationParams : register(b0)
{
    float2 gScreenCenter;
    float gAberrationIntensity;
    uint gPadding;
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

[earlydepthstencil]
float4 PS(VertexOut pIn) : SV_TARGET
{   
    // Chromatic Aberration
    float2 coords = 2.0 * pIn.TexC - 1.0;
    float2 end = pIn.TexC - coords * dot(coords, coords) * gAberrationIntensity;
    float2 delta = (end - pIn.TexC) / 3.0;
    
    float red = gAccumTexture.Sample(gsamPointWrap, pIn.TexC).r;
    float green = gAccumTexture.Sample(gsamLinearClamp, delta + pIn.TexC).g;
    float blue = gAccumTexture.Sample(gsamLinearClamp, delta * 2.0 + pIn.TexC).b;
    
    float3 hdrColor = float3(red, green, blue);
    float3 bloomColor = gBloomTexture.Sample(gsamPointWrap, pIn.TexC).rgb;
    
    hdrColor += 1.0f * bloomColor;
    
    float gamma = 2.2;
  
    // reinhard tone mapping
    float3 mapped = hdrColor / (hdrColor + (1.0f));
    mapped = pow(mapped, (1.0 / gamma));
    
    return float4(mapped, 1.0);
}