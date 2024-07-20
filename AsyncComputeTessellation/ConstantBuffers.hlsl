#ifndef CONSTANT_BUFFERS
#define CONSTANT_BUFFERS

cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float aspectRatio;
    uint3 padding0;
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