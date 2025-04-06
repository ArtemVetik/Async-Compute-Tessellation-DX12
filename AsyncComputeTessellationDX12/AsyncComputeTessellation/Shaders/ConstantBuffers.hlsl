#ifndef CONSTANT_BUFFERS
#define CONSTANT_BUFFERS

cbuffer objectData : register(b0)
{
    matrix gWorld;
    matrix gTexTransform;
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
    uint gIndicesCount;
    uint gTrianglesCount;
    uint2 gPadding0;
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

cbuffer shadowMapData : register(b3)
{
    matrix gShadowViewProj;
    float3 gLightPos;
    float gPadding1;
}

#endif