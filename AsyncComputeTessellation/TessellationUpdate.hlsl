
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

[numthreads(32, 1, 1)]
void main(uint id : SV_DispatchThreadID)
{
    if (id.x >= 100)
        return;
    
    VertexPool[id.x] = id.x;

	uint drawIndex = DrawList.IncrementCounter();
    DrawList[drawIndex] = id.x;
}
