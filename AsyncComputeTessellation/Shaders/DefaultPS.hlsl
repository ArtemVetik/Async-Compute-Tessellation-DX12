#define COMPUTE_SHADER 0

#include "Common.hlsl"
#include "ConstantBuffers.hlsl"
#include "Noise.hlsl"

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
    pin.NormalW = normalize(float3(-s * gDisplaceFactor / 2.0, 1));
#endif    
    
    float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
    shadowFactor[0] = 1;
    
    output.albedo = 1;
    output.normal = float4(pin.NormalW, shadowFactor[0]);
    
    return output;
}
