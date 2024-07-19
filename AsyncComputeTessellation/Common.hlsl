#if COMPUTE_SHADER
#include "ComputeShaderData.hlsl"
#else
#include "DefaultShaderData.hlsl"
#endif

struct Triangle
{
    float3 Vertex[3];
};

uint ts_findMSB_64(uint2 nodeID)
{
    return nodeID.x == 0 ? firstbithigh(nodeID.y) : (firstbithigh(nodeID.x) + 32);
}

bool ts_isLeaf_64(uint2 nodeID)
{
    return ts_findMSB_64(nodeID) == 63u;
}

bool ts_isRoot_64(uint2 nodeID)
{
    return ts_findMSB_64(nodeID) == 0u;
}

bool ts_isZeroChild_64(uint2 nodeID)
{
    return (nodeID.y & 1u) == 0u;
}

uint2 ts_leftShift_64(uint2 nodeID, uint shift)
{
    uint2 result = nodeID;
    //Extract the "shift" first bits of y and append them at the end of x
    result.x = result.x << shift;
    result.x |= result.y >> (32u - shift);
    result.y = result.y << shift;
    return result;
}

uint2 ts_rightShift_64(uint2 nodeID, uint shift)
{
    uint2 result = nodeID;
    //Extract the "shift" last bits of x and prepend them to y
    result.y = result.y >> shift;
    result.y |= result.x << (32u - shift);
    result.x = result.x >> shift;
    return result;
}

void ts_children_64(uint2 nodeID, out uint2 children[2])
{
    nodeID = ts_leftShift_64(nodeID, 1u);
    children[0] = uint2(nodeID.x, nodeID.y | 0u);
    children[1] = uint2(nodeID.x, nodeID.y | 1u);
}

uint2 ts_parent_64(uint2 nodeID)
{
    return ts_rightShift_64(nodeID, 1u);
}

float3x2 ts_mul(float3x2 A, float3x2 B)
{
    float2x2 tmpA = float2x2(A[0][0], A[0][1], A[1][0], A[1][1]);
    float2x2 tmpB = float2x2(B[0][0], B[0][1], B[1][0], B[1][1]);
    
    float2x2 tmp = mul(tmpA, tmpB);
    
    float3x2 r;
    r[0] = float2(tmp[0][0], tmp[0][1]);
    r[1] = float2(tmp[1][0], tmp[1][1]);
    
    float2x3 T = transpose(float3x2(A[0], A[1], A[2]));
    
    r[2].x = dot(T[0], float3(B[2][0], B[2][1], 1.0f));
    r[2].y = dot(T[1], float3(B[2][0], B[2][1], 1.0f));

    return r;
}

float3x2 jk_bitToMatrix(in uint bit)
{
    float s = float(bit) - 0.5;
    float2 r1 = float2(-0.5, +s);
    float2 r2 = float2(-s, -0.5);
    float2 r3 = float2(+0.5, +0.5);
    return float3x2(r1, r2, r3);
}

void ts_getMeshTriangle(uint meshPolygonID, out Triangle t)
{
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        t.Vertex[i] = MeshDataVertex.Load(MeshDataIndex.Load(meshPolygonID + i));
    }
}

void ts_getTriangleXform_64(uint2 nodeID, out float3x2 xform, out float3x2 parent_xform)
{
    float2 r1 = float2(1, 0);
    float2 r2 = float2(0, 1);
    float2 r3 = float2(0, 0);
    float3x2 xf = float3x2(r1, r2, r3);

    // Handles the root triangle case
    if (nodeID.x == 0u && nodeID.y == 1u)
    {
        xform = parent_xform = xf;
        return;
    }

    uint lsb = nodeID.y & 1u;
    nodeID = ts_rightShift_64(nodeID, 1u);
    while (nodeID.x > 0 || nodeID.y > 1)
    {
        xf = ts_mul(jk_bitToMatrix(nodeID.y & 1u), xf);
        nodeID = ts_rightShift_64(nodeID, 1u);
    }

    parent_xform = xf;
    xform = ts_mul(parent_xform, jk_bitToMatrix(lsb & 1u));
}

float2 ts_Leaf_to_Tree_64(float2 p, uint2 nodeID)
{
    float3x2 xform, pxform;
    ts_getTriangleXform_64(nodeID, xform, pxform);
    return mul(float3(p, 1), xform).xy;
}

float3 ts_mapTo3DTriangle(Triangle t, float2 uv)
{
    float3 result = (1.0 - uv.x - uv.y) * t.Vertex[0] +
            uv.x * t.Vertex[2] +
            uv.y * t.Vertex[1];
    return result;
}

float3 ts_Tree_to_MeshPosition(float2 p, uint meshPolygonID)
{
    Triangle mesh_t;
    ts_getMeshTriangle(meshPolygonID, mesh_t);
    return ts_mapTo3DTriangle(mesh_t, p);
}

float3 ts_Leaf_to_MeshPosition(float2 p, uint4 key)
{
    uint2 nodeID = key.xy;
    uint meshPolygonID = key.z;
    float2 p2d = ts_Leaf_to_Tree_64(p, nodeID);
    return ts_Tree_to_MeshPosition(p2d, meshPolygonID);
}

void ts_Leaf_n_Parent_to_MeshPosition(float2 p, uint4 key, out float3 p_mesh, out float3 pp_mesh)
{
    uint2 nodeID = key.xy;
    uint meshPolygonID = key.z;
    float3x2 xf, pxf;
    float2 p2D, pp2D;

    ts_getTriangleXform_64(nodeID, xf, pxf);
    p2D = mul(float3(p, 1), xf).xy;
    pp2D = mul(float3(p, 1), pxf).xy;

    p_mesh = ts_Tree_to_MeshPosition(p2D, meshPolygonID);
    pp_mesh = ts_Tree_to_MeshPosition(pp2D, meshPolygonID);
}