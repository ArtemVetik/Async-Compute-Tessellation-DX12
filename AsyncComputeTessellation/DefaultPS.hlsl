
cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float3 camPosition;
    float aspectRatio;
};

struct VertexIn
{
	float3 PosL    : POSITION;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    return float4(pin.TexC, 1, 1);
}


