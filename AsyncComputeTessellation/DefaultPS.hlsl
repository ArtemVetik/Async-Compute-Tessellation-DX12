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

float4 main(VertexOut pin) : SV_Target
{
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(camPosition - pin.PosW);

	// Indirect lighting.
    float4 gAmbientLight = float4(0.55f, 0.55f, 0.55f, 1.0f);
    float4 gDiffuseAlbedo = float4(0.8f, 0.8f, 0.8f, 1.0f);
    float gRoughness = 0.125f;
    float3 gFresnelR0 = float3(0.02f, 0.02f, 0.02f);
    
    float4 ambient = gAmbientLight * gDiffuseAlbedo;
    
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
    
    const float shininess = 1.0f - gRoughness;
    Material mat = { gDiffuseAlbedo, gFresnelR0, shininess };
    
    float4 directLight = ComputeLighting(lights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse material.
    litColor.a = gDiffuseAlbedo.a;
    
    
    return litColor;
}
