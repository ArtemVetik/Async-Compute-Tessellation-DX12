#define COMPUTE_SHADER 0

#include "Common.hlsl"

float4 main(VertexOut pin) : SV_Target
{
    return float4(pin.TexC, 1, 1);
}


