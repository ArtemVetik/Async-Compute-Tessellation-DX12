#define COMPUTE_SHADER 0

#include "Common.hlsl"

[maxvertexcount(3)]
void main(triangle VertexOut input[3], inout TriangleStream<VertexOut> outputStream)
{
    VertexOut output;

    for (int i = 0; i < 3; ++i)
    {
        output = input[i];
        output.LeafPos = float2(i >> 1, i & 1);
        
        outputStream.Append(output);
    }
    
    outputStream.RestartStrip();
}