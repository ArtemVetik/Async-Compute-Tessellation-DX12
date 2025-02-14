#if COMPUTE_SHADER
#include "ComputeShaderData.hlsl"
#else
#include "DefaultShaderData.hlsl"
#endif
#include "ConstantBuffers.hlsl"

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

FTYPE3x2 ts_mul(FTYPE3x2 A, FTYPE3x2 B)
{
    FTYPE2x2 tmpA = FTYPE2x2(A[0][0], A[0][1], A[1][0], A[1][1]);
    FTYPE2x2 tmpB = FTYPE2x2(B[0][0], B[0][1], B[1][0], B[1][1]);
    
    FTYPE2x2 tmp = mul(tmpA, tmpB);
    
    FTYPE3x2 r;
    r[0] = FTYPE2(tmp[0][0], tmp[0][1]);
    r[1] = FTYPE2(tmp[1][0], tmp[1][1]);
    
    FTYPE2x3 T = transpose(FTYPE3x2(A[0], A[1], A[2]));
    
    r[2].x = dot(T[0], FTYPE3(B[2][0], B[2][1], 1.0f));
    r[2].y = dot(T[1], FTYPE3(B[2][0], B[2][1], 1.0f));

    return r;
}

FTYPE3x2 jk_bitToMatrix(in uint bit)
{
    FTYPE s = FTYPE(bit) - 0.5;
    FTYPE2 r1 = FTYPE2(-0.5, +s);
    FTYPE2 r2 = FTYPE2(-s, -0.5);
    FTYPE2 r3 = FTYPE2(+0.5, +0.5);
    return FTYPE3x2(r1, r2, r3);
}

void ts_getMeshTriangle(uint meshPolygonID, out Triangle t)
{
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        t.Vertex[i] = MeshDataVertex.Load(MeshDataIndex.Load(meshPolygonID + i));
    }
}

void ts_getTriangleXform_64(uint2 nodeID, out FTYPE3x2 xform, out FTYPE3x2 parent_xform)
{
    FTYPE2 r1 = FTYPE2(1, 0);
    FTYPE2 r2 = FTYPE2(0, 1);
    FTYPE2 r3 = FTYPE2(0, 0);
    FTYPE3x2 xf = FTYPE3x2(r1, r2, r3);

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

FTYPE2 ts_Leaf_to_Tree_64(FTYPE2 p, uint2 nodeID)
{
    FTYPE3x2 xform, pxform;
    ts_getTriangleXform_64(nodeID, xform, pxform);
    return mul(FTYPE3(p, 1), xform).xy;
}

FTYPE3 ts_mapTo3DTriangle(Triangle t, FTYPE2 uv)
{
    FTYPE3 result = (1.0 - uv.x - uv.y) * t.Vertex[0].Position +
            uv.x * t.Vertex[2].Position +
            uv.y * t.Vertex[1].Position;
    return result;
}

Vertex ts_interpolateVertex(Triangle t, FTYPE2 uv)
{
    Vertex v;
    v.Position = (1.0 - uv.x - uv.y) * t.Vertex[0].Position
            + uv.x * t.Vertex[2].Position
            + uv.y * t.Vertex[1].Position;
    v.Normal = (1.0 - uv.x - uv.y) * t.Vertex[0].Normal
            + uv.x * t.Vertex[2].Normal
            + uv.y * t.Vertex[1].Normal;
    v.Normal = normalize(v.Normal);
    v.TexC = (1.0 - uv.x - uv.y) * t.Vertex[0].TexC
            + uv.x * t.Vertex[2].TexC
            + uv.y * t.Vertex[1].TexC;
    v.TangentU = (1.0 - uv.x - uv.y) * t.Vertex[0].TangentU
            + uv.x * t.Vertex[2].TangentU
            + uv.y * t.Vertex[1].TangentU;
    return v;
}

FTYPE3 ts_Tree_to_MeshPosition(FTYPE2 p, uint meshPolygonID)
{
    Triangle mesh_t;
    ts_getMeshTriangle(meshPolygonID, mesh_t);
    return ts_mapTo3DTriangle(mesh_t, p);
}

FTYPE3 ts_Leaf_to_MeshPosition(FTYPE2 p, uint4 key)
{
    uint2 nodeID = key.xy;
    uint meshPolygonID = key.z;
    FTYPE2 p2d = ts_Leaf_to_Tree_64(p, nodeID);
    return ts_Tree_to_MeshPosition(p2d, meshPolygonID);
}

void ts_Leaf_n_Parent_to_MeshPosition(FTYPE2 p, uint4 key, out FTYPE3 p_mesh, out FTYPE3 pp_mesh)
{
    uint2 nodeID = key.xy;
    uint meshPolygonID = key.z;
    FTYPE3x2 xf, pxf;
    FTYPE2 p2D, pp2D;

    ts_getTriangleXform_64(nodeID, xf, pxf);
    p2D = mul(FTYPE3(p, 1), xf).xy;
    pp2D = mul(FTYPE3(p, 1), pxf).xy;

    p_mesh = ts_Tree_to_MeshPosition(p2D, meshPolygonID);
    pp_mesh = ts_Tree_to_MeshPosition(pp2D, meshPolygonID);
}