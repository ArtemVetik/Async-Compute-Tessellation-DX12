#include "SimplexNoise.hlsl"

#define u_displace_factor 10
#define H 0.96
#define lacunarity 1.99
#define sharpness 0.02

float displace(float2 p, float screen_resolution)
{
    p *= sharpness;
    const float max_octaves = 16.0;
    float frequency = 1.5;
    float octaves = clamp(log2(screen_resolution) - 2.0, 0.0, max_octaves);
    float value = 0.0;

    for (float i = 0.0; i < octaves - 1.0; i += 1.0)
    {
        value += snoise(p) * pow(frequency, -H);
        p *= lacunarity;
        frequency *= lacunarity;
    }
    value += frac(octaves) * snoise(p) * pow(frequency, -H);
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
        value += v * pow(frequency, -H)
		      * float3(1, float2(pow(lacunarity, i), 0));
        p *= lacunarity;
        frequency *= lacunarity;
    }
    value += frac(octaves) * snoise3D(float3(p.x, p.y, 0))
	      * pow(frequency, -H) * float3(1, pow(lacunarity, octaves).xy);
    gradient = value.yz;
    return value.x;
}


float3 displaceVertex(float3 v, float3 eye)
{
    float f = 2e4 / distance(v, eye);
    v.y = displace(v.xz, f) * u_displace_factor;
    return v;
}

float4 displaceVertex(float4 v, float3 eye)
{
    return float4(displaceVertex(v.xyz, eye), v.w);
}

float getHeight(float2 v, float f)
{
    return displace(v, f) * u_displace_factor;
}