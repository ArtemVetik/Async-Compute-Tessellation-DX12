#ifndef CONSTANT_BUFFERS
#define CONSTANT_BUFFERS

#include "LightingUtil.hlsl"

cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float aspectRatio;
    uint3 padding0;
    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light lights[MaxLights];
};

cbuffer tessellationData : register(b1)
{
    matrix meshWorld;
    uint subdivisionLevel;
    uint screenRes;
    float displaceFactor;
    uint wavesAnimationFlag;
    float displaceLacunarity;
    float displacePosScale;
    float displaceH;
    float lodFactor;
};

cbuffer perFrameData : register(b2)
{
    float3 camPosition;
    float deltaTime;
    float totalTime;
    uint3 padding2;
}

#endif