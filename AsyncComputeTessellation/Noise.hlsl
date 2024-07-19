#include "ConstantBuffers.hlsl"
#include "SimplexNoise.hlsl"

float displace(float2 p, float screen_resolution)
{
    p *= displacePosScale;
    p += totalTime * 0.5 * wavesAnimationFlag;
    
    const float max_octaves = 16.0;
    float frequency = 1.5;
    float octaves = clamp(log2(screen_resolution) - 2.0, 0.0, max_octaves);
    float value = 0.0;

    for (float i = 0.0; i < octaves - 1.0; i += 1.0)
    {
        value += snoise(p) * pow(frequency, -displaceH);
        p *= displaceLacunarity;
        frequency *= displaceLacunarity;
    }
    value += frac(octaves) * snoise(p) * pow(frequency, -displaceH);
    return value;
}

float displace(float2 p, float screen_resolution, out float2 gradient)
{
    const float max_octaves = 24.0;
    float frequency = 1.5;
    float3 octaves = clamp(log2(screen_resolution) - 2.0, 0.0, max_octaves);
    float3 value = float3(0, 0, 0);

    for (int i = 0; i < (int) (octaves - 1); i += 1)
    {
        float3 v = snoise3D(float3(p.x, p.y, 0));
        value += v * pow(frequency, -displaceH)
		      * float3(1, float2(pow(displaceLacunarity, i), 0));
        p *= displaceLacunarity;
        frequency *= displaceLacunarity;
    }
    value += frac(octaves) * snoise3D(float3(p.x, p.y, 0))
	      * pow(frequency, -displaceH) * float3(1, pow(displaceLacunarity, octaves).xy);
    gradient = value.yz;
    return value.x;
}


float3 displaceVertex(float3 v, float3 eye)
{
    float f = 2e4 / distance(v, eye);
    v.y = displace(v.xz, f) * displaceFactor;
    return v;
}

float4 displaceVertex(float4 v, float3 eye)
{
    return float4(displaceVertex(v.xyz, eye), v.w);
}

float getHeight(float2 v, float f)
{
    return displace(v, f) * displaceFactor;
}