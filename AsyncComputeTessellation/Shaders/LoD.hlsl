#include "ConstantBuffers.hlsl"
#include "ComputeShaderData.hlsl"
#include "Common.hlsl"

static const FTYPE2 triangle_centroid = FTYPE2(0.5, 0.5);

FTYPE distanceToLod(FTYPE3 pos)
{
    FTYPE d = distance(pos, gPredictedCamPosition);
    FTYPE lod = (d * gLodFactor);
    lod = clamp(lod, 0.0, 1.0);
    return -2.0 * log2(lod);
}

void computeTessLvlWithParent(uint4 key, FTYPE height, out FTYPE lvl, out FTYPE parent_lvl)
{   
    FTYPE3 p_mesh, pp_mesh;
    ts_Leaf_n_Parent_to_MeshPosition(triangle_centroid, key, p_mesh, pp_mesh);
    p_mesh = mul(FTYPE4(p_mesh, 1), gWorld).xyz;
    pp_mesh = mul(FTYPE4(pp_mesh, 1), gWorld).xyz;
    p_mesh.y = height;
    pp_mesh.y = height;

    lvl = distanceToLod(p_mesh.xyz);
    parent_lvl = distanceToLod(pp_mesh.xyz);
}

void computeTessLvlWithParent(uint4 key, out FTYPE lvl, out FTYPE parent_lvl)
{
    FTYPE3 p_mesh, pp_mesh;
    
    ts_Leaf_n_Parent_to_MeshPosition(triangle_centroid, key, p_mesh, pp_mesh);
    p_mesh = mul(FTYPE4(p_mesh, 1), gWorld).xyz;
    pp_mesh = mul(FTYPE4(pp_mesh, 1), gWorld).xyz;

    lvl = distanceToLod(p_mesh.xyz);
    parent_lvl = distanceToLod(pp_mesh.xyz);
}

bool culltest(FTYPE4x4 mvp, FTYPE3 bmin, FTYPE3 bmax)
{
    bool inside = true;
    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        bool3 b = (gFrustrumPlanes[i].xyz > FTYPE3(0, 0, 0));
        FTYPE3 n = lerp(bmin, bmax, b);
        inside = inside && (dot(FTYPE4(n, 1.0), gFrustrumPlanes[i]) >= 0);
    }
    return inside;
}