
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
    float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION;
    float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct ps_output
{
    float4 albedo : SV_TARGET0;
    float3 normal : SV_TARGET1;
};

ps_output main(VertexOut pin) : SV_Target
{
    ps_output output;
    
    output.albedo = float4(pin.TexC, 1, 1);
    output.normal = pin.NormalW;
    
    return output;
}


