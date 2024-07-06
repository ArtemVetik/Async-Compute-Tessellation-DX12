
cbuffer objectData : register(b0)
{
    matrix world;
    matrix view;
    matrix projection;
    float aspectRatio;
};

RWStructuredBuffer<float3> VertexPool : register(u0);
RWStructuredBuffer<uint> DrawList : register(u1);
RWStructuredBuffer<uint> DrawArgs : register(u2);

[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    DrawArgs[0] = DrawList.IncrementCounter(); // vertexCountPerInstance (or index count if using an index buffer)
    DrawArgs[1] = 1; // instanceCount
    DrawArgs[2] = 0; // offsets
    DrawArgs[3] = 0; // offsets
    DrawArgs[4] = 0; // offsets
    DrawArgs[5] = 0; // offsets
    DrawArgs[6] = 0; // offsets
    DrawArgs[7] = 0; // offsets
    DrawArgs[8] = 0; // offsets
}