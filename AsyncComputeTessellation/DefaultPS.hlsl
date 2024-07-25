#define COMPUTE_SHADER 0

#include "Common.hlsl"
#include "LightingUtil.hlsl"
#include "Noise.hlsl"

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
    // Complete projection by doing division by w.
    shadowPosH.xyz /= shadowPosH.w;

    // Depth in NDC space.
    float depth = shadowPosH.z;

    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);

    // Texel size.
    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow,
            shadowPosH.xy + offsets[i], depth).r;
    }
    
    return percentLit / 9.0f;
}

ps_output main(VertexOut pin) : SV_Target
{
    ps_output output;
    
    pin.NormalW = normalize(pin.NormalW);
    
    float3 dx = ddx(pin.PosW);
    float3 dy = ddy(pin.PosW);
#if FLAT_NORMALS
    pin.NormalW = normalize(cross(dx, dy)); 
#elif USE_DISPLACE
    float dp = sqrt(dot(dx, dx));
    float2 s;
    float d = displace(pin.PosW.xz, 100 / (0.5 * dp), s);
    pin.NormalW = normalize(float3(-s * displaceFactor / 2.0, 1));
#endif    
    
    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
    
    output.albedo = 1;
    output.normal = float4(pin.NormalW, shadowFactor[0]);
    
    return output;
}
