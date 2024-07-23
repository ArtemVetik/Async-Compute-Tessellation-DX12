#define COMPUTE_SHADER 0

#include "Common.hlsl"
#include "LightingUtil.hlsl"
#include "Noise.hlsl"

float4 main(VertexOut pin) : SV_Target
{
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);

    // Vector from point being lit to eye. 
    float3 toEyeW = normalize(camPosition - pin.PosW);

	// Indirect lighting.
    float4 gAmbientLight = float4(0.55f, 0.55f, 0.55f, 1.0f);
    float4 gDiffuseAlbedo = float4(0.7f, 0.7f, 0.7f, 1.0f);
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
    
    const float shininess = 1.0f - gRoughness;
    Material mat = { gDiffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 directLight = ComputeLighting(lights, mat, pin.PosW,
        pin.NormalW, toEyeW, shadowFactor);

    float4 litColor = ambient + directLight;

    // Common convention to take alpha from diffuse material.
    litColor.a = gDiffuseAlbedo.a;
    
    
    return litColor;
}
