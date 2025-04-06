#ifndef STRUCTS
#define STRUCTS

#if USE_FP16
#define FTYPE min16float
#define FTYPE2 min16float2
#define FTYPE3 min16float3
#define FTYPE4 min16float4
#define FTYPE2x2 min16float2x2
#define FTYPE3x3 min16float3x3
#define FTYPE4x4 min16float4x4
#define FTYPE3x2 min16float3x2
#define FTYPE2x3 min16float2x3
#else
#define FTYPE float
#define FTYPE2 float2
#define FTYPE3 float3
#define FTYPE4 float4
#define FTYPE2x2 float2x2
#define FTYPE3x3 float3x3
#define FTYPE4x4 float4x4
#define FTYPE3x2 float3x2
#define FTYPE2x3 float2x3
#endif

struct Vertex
{
    float4 Position;
    float4 Normal;
    float4 TangentU;
    float4 TexC;
};

struct Triangle
{
    Vertex Vertex[3];
};

#endif