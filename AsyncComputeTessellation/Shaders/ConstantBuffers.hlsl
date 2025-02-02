#ifndef CONSTANT_BUFFERS
#define CONSTANT_BUFFERS

cbuffer objectData : register(b0)
{
    matrix gWorld;
};

cbuffer tessellationData : register(b1)
{
    uint gSubdivisionLevel;
    uint  gScreenRes;
    float gDisplaceFactor;
    uint  gWavesAnimationFlag;
    float gDisplaceLacunarity;
    float gDisplacePosScale;
    float gDisplaceH;
    float gLodFactor;
};

cbuffer perFrameData : register(b2)
{
    matrix gViewProj;
    float3 gCamPosition;
    float  gDeltaTime;
    float3 gPredictedCamPosition;
    float  gTotalTime;
    float4 gFrustrumPlanes[6];
}

#endif