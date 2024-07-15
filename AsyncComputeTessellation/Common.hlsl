
uint ts_findMSB_64(uint2 nodeID)
{
    return nodeID.x == 0 ? firstbithigh(nodeID.y) : firstbithigh(nodeID.x + 32);
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

float3x3 jk_bitToMatrix(in uint bit)
{
    float s = float(bit) - 0.5;
    float3 c1 = float3(+s, -0.5, 0);
    float3 c2 = float3(-0.5, -s, 0);
    float3 c3 = float3(+0.5, +0.5, 1);
    return float3x3(c1, c2, c3);
}

void ts_getTriangleXform_64(uint2 nodeID, out float3x2 xform, out float3x2 parent_xform)
{
    float2 c1 = float2(1, 0);
    float2 c2 = float2(0, 1);
    float2 c3 = float2(0, 0);
    float3x2 xf = float3x2(c1, c2, c3);

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
        xf = mul(jk_bitToMatrix(nodeID.y & 1u), xf);
        nodeID = ts_rightShift_64(nodeID, 1u);
    }

    parent_xform = xf;
    xform = mul(jk_bitToMatrix(lsb & 1u), parent_xform);
}

float2 ts_Leaf_to_Tree_64(float2 p, uint2 nodeID)
{
    float3x2 xform, pxform;
    ts_getTriangleXform_64(nodeID, xform, pxform);
    return mul(float3(p, 1), xform);
}