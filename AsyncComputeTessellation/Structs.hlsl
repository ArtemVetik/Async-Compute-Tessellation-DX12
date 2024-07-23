#ifndef STRUCTS
#define STRUCTS

struct Vertex
{
    float3 Position;
    float3 Normal;
    float3 TangentU;
    float2 TexC;
};

struct Triangle
{
    Vertex Vertex[3];
};

#endif