#include "ConstantBuffers.hlsl"
#include "gpu_noise_lib.hlsl"

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
        value += SimplexPerlin2D(p) * pow(frequency, -displaceH);
        p *= displaceLacunarity;
        frequency *= displaceLacunarity;
    }
    value += frac(octaves) * SimplexPerlin2D(p) * pow(frequency, -displaceH);
    return value;
}

float displace(float2 p, float screen_resolution, out float2 gradient)
{   
    p *= displacePosScale;
    p += totalTime * 0.5 * wavesAnimationFlag;
    const float max_octaves = 16.0;
    float frequency = 1.5;
    float octaves = clamp(log2(screen_resolution) - 2.0, 0.0, max_octaves);
    float3 value = 0;

    for (float i = 0.0; i < octaves - 1.0; i += 1.0)
    {
        float3 v = SimplexPerlin2D_Deriv(p);
        float diPow = pow(displaceLacunarity, i);
        value += v * pow(frequency, -displaceH)
		      * float3(1, float2(diPow, diPow));
        p *= displaceLacunarity;
        frequency *= displaceLacunarity;
    }
    float doPow = pow(displaceLacunarity, octaves);
    value += frac(octaves) * SimplexPerlin2D_Deriv(p)
	      * pow(frequency, -displaceH) * float3(1, float2(doPow, doPow));
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