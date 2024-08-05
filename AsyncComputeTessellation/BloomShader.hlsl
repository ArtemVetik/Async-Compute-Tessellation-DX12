Texture2D gAccumTexture : register(t0);
Texture2D gBloomTexture : register(t1);

StructuredBuffer<float> gWeights : register(t2);

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
    float3 padding;
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

float4 PS(VertexOut pIn) : SV_Target
{
    float3 lightColor = gAccumTexture.Sample(gsamLinearClamp, pIn.TexC);

    float luminance = dot(lightColor, float3(0.2126, 0.7152, 0.0722));
    float3 thresholdColor = step(gThreshold, luminance) * lightColor;
    return float4(thresholdColor, 1.0f);
}

float4 PSMain(VertexOut pIn) : SV_Target
{
    float4 color = gBloomTexture.SampleLevel(gsamLinearClamp, pIn.TexC, 0) * gWeights[0];
    float width, height;
    gBloomTexture.GetDimensions(width, height);

#if HORIZONTAL_BLUR
    float2 texelSize = float2(1.0f/width, 0.0f);
#else
    float2 texelSize = float2(0.0f, 1.0f / height);
#endif
    
    [unroll]
    for (int i = 1; i <= 7; i++)
    {
        color += gBloomTexture.SampleLevel(gsamLinearClamp, pIn.TexC + texelSize * i, 0) * gWeights[i];
        color += gBloomTexture.SampleLevel(gsamLinearClamp, pIn.TexC - texelSize * i, 0) * gWeights[i];
    }
    return color;
}
