#include "ComputeShaderData.hlsl"
#include "Common.hlsl"

static const float2 triangle_centroid = float2(0.5, 0.5);

float distanceToLod(float3 pos)
{
    float d = distance(pos, camPosition);
    float lod = (d * lodFactor);
    lod = clamp(lod, 0.0, 1.0);
    return -2.0 * log2(lod);
}

void computeTessLvlWithParent(uint4 key, float height, out float lvl, out float parent_lvl)
{
    float3 p_mesh, pp_mesh;
    ts_Leaf_n_Parent_to_MeshPosition(triangle_centroid, key, p_mesh, pp_mesh);
    p_mesh = mul(float4(p_mesh, 1), meshWorld);
    pp_mesh = mul(float4(pp_mesh, 1), meshWorld);
    p_mesh.y = height;
    pp_mesh.y = height;

    lvl = distanceToLod(p_mesh.xyz);
    parent_lvl = distanceToLod(pp_mesh.xyz);
}

void computeTessLvlWithParent(uint4 key, out float lvl, out float parent_lvl)
{
    float3 p_mesh, pp_mesh;
    
    ts_Leaf_n_Parent_to_MeshPosition(triangle_centroid, key, p_mesh, pp_mesh);
    p_mesh = mul(float4(p_mesh, 1), meshWorld);
    pp_mesh = mul(float4(pp_mesh, 1), meshWorld);

    lvl = distanceToLod(p_mesh.xyz);
    parent_lvl = distanceToLod(pp_mesh.xyz);
}