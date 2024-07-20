#define COMPUTE_SHADER 0

#include "Common.hlsl"

float4 levelColor(uint lvl)
{
    float4 c = float4(0.5, 0.5, 0.5, 1);
    uint mod = lvl % 4;
    if (mod == 0)
    {
        c.r += 0.5;
    }
    else if (mod == 1)
    {
        c.g += 0.5;
    }
    else if (mod == 2)
    {
        c.b += 0.5;
    }
    return c;
}

float gridFactor(float2 vBC, float width)
{
    float3 bary = float3(vBC.x, vBC.y, 1.0 - vBC.x - vBC.y);
    float3 d = fwidth(bary);
    float3 a3 = smoothstep(d * (width - 0.5), d * (width + 0.5), bary);
    return min(min(a3.x, a3.y), a3.z);
}

float4 main(VertexOut pin) : SV_Target
{
    float3 p = pin.PosW;
    
    float4 c = levelColor(pin.Lvl);

    float wireframe_factor = gridFactor(pin.LeafPos, 0.5);

    return float4(c.xyz * wireframe_factor, 1);
}
