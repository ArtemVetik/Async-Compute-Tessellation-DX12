Texture2D gBloomTexture0 : register(t0);
Texture2D gBloomTexture1 : register(t1);

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbBloomPass : register(b0)
{
    float gThreshold;
    float gIntensity;
    float gScatter;
    float gPadding0;
    float3 gTint;
    float gPadding1;
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

float4 PSThreshold(VertexOut pIn) : SV_Target
{
    float3 lightColor = gBloomTexture0.Sample(gsamLinearClamp, pIn.TexC).rgb;

    float luminance = dot(lightColor, float3(0.2126, 0.7152, 0.0722));
    float3 thresholdColor = step(gThreshold, luminance) * lightColor;
    return float4(thresholdColor, 1.0f);
}

float4 PSBlurH(VertexOut pIn) : SV_Target
{
    float width, height;
    gBloomTexture1.GetDimensions(width, height);
    
    float2 texelSize = float2(1.0f / width, 0.0f);
    
    float3 c0 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 4.0, 0.0));
    float3 c1 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 3.0, 0.0));
    float3 c2 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 2.0, 0.0));
    float3 c3 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC - float2(texelSize.x * 1.0, 0.0));
    float3 c4 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC);
    float3 c5 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 1.0, 0.0));
    float3 c6 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 2.0, 0.0));
    float3 c7 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 3.0, 0.0));
    float3 c8 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC + float2(texelSize.x * 4.0, 0.0));
    
    float3 color = c0 * 0.01621622 + c1 * 0.05405405 + c2 * 0.12162162 + c3 * 0.19459459
                 + c4 * 0.22702703
                 + c5 * 0.19459459 + c6 * 0.12162162 + c7 * 0.05405405 + c8 * 0.01621622;
    
    return float4(color, 1.0f);
}

float4 PSBlurV(VertexOut pIn) : SV_Target
{
    float width, height;
    gBloomTexture1.GetDimensions(width, height);
    
    float2 texelSize = float2(0.0f, 1.0f / height);

    // Optimized bilinear 5-tap gaussian on the same-sized source (9-tap equivalent)
    float3 c0 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC - float2(0, texelSize.y * 3.23076923));
    float3 c1 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC - float2(0, texelSize.y * 1.38461538));
    float3 c2 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC);
    float3 c3 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC + float2(0, texelSize.y * 1.38461538));
    float3 c4 = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC + float2(0, texelSize.y * 3.23076923));
    
    float3 color = c0 * 0.07027027 + c1 * 0.31621622
                 + c2 * 0.22702703
                 + c3 * 0.31621622 + c4 * 0.07027027;
    
    return float4(color, 1.0f);
}

float4 PSUpscale(VertexOut pIn) : SV_Target
{
    float4 highMip = gBloomTexture0.Sample(gsamLinearClamp, pIn.TexC);
    float4 lowMip = gBloomTexture1.Sample(gsamLinearClamp, pIn.TexC);
    
    return lerp(highMip, lowMip, gScatter) * gIntensity * float4(gTint, 1.0f);
}