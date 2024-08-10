#define COMPUTE_SHADER 1

#include "LoD.hlsl"
#include "Noise.hlsl"

static const int O = 0;
static const int R = 1;
static const int U = 2;
static const float2 unit_O = float2(0, 0);
static const float2 unit_R = float2(1, 0);
static const float2 unit_U = float2(0, 1);

void cull_writeKey(uint4 key)
{
    uint idx;
    InterlockedAdd(SubdCounter[2], 1, idx);
    SubdBufferOutCulled[idx] = key;
}

void cullPass(uint4 key)
{
    float3x3 mesh_coord;
    float3 b_min = 10e6;
    float3 b_max = -10e6;

    mesh_coord[O] = ts_Leaf_to_MeshPosition(unit_O, key);
    mesh_coord[U] = ts_Leaf_to_MeshPosition(unit_U, key);
    mesh_coord[R] = ts_Leaf_to_MeshPosition(unit_R, key);
    
#if USE_DISPLACE
    mesh_coord[O] = displaceVertex(mesh_coord[O], predictedCamPosition);
    mesh_coord[U] = displaceVertex(mesh_coord[U], predictedCamPosition);
    mesh_coord[R] = displaceVertex(mesh_coord[R], predictedCamPosition);
#endif

    b_min = min(b_min, mesh_coord[O]);
    b_min = min(b_min, mesh_coord[U]);
    b_min = min(b_min, mesh_coord[R]);

    b_max = max(b_max, mesh_coord[O]);
    b_max = max(b_max, mesh_coord[U]);
    b_max = max(b_max, mesh_coord[R]);
    
    float4x4 mvp = mul(mul(world, view), projection);
    if (culltest(mvp, b_min.xyz, b_max.xyz))
        cull_writeKey(key);
}
void compute_writeKey(uint2 new_nodeID, uint4 current_key)
{
    uint4 new_key = uint4(new_nodeID, current_key.zw);
    uint idx;
    InterlockedAdd(SubdCounter[1], 1, idx);
    SubdBufferOut[idx] = new_key;
}
[numthreads(512, 1, 1)]
void main(uint id : SV_DispatchThreadID, uint groupId : SV_GroupIndex)
{
    uint4 key = SubdBufferIn[id.x];
    uint2 nodeID = key.xy;
    
#if USE_DISPLACE
    // When subdividing heightfield, we set the plane height to the heightmap
    // value under the camera for more fidelity.
    // To avoid computing the procedural height value in each instance, we
    // store it in a shared variable
    if (groupId == 0)
    {
        cam_height_local = getHeight(predictedCamPosition.xz, screenRes);
    }
    GroupMemoryBarrierWithGroupSync();
#endif
    
    if (id.x >= SubdCounter[0])
        return;
    
    int targetLod = 0, parentLod = 0;
#if UNIFORM_TESSELLATION
    targetLod = subdivisionLevel;
    parentLod = subdivisionLevel;
#else 
    float parentTargetLevel, targetLevel;
#if USE_DISPLACE
    computeTessLvlWithParent(key, cam_height_local, targetLevel, parentTargetLevel);
#else
    computeTessLvlWithParent(key, targetLevel, parentTargetLevel);
#endif
    targetLod = int(targetLevel);
    parentLod = int(parentTargetLevel);
#endif
    
    int keyLod = ts_findMSB_64(nodeID);
    
    // update the key accordingly
    if ( /* subdivide ? */keyLod < targetLod && !ts_isLeaf_64(nodeID))
    {
        uint2 children[2];
        ts_children_64(nodeID, children);
        compute_writeKey(children[0], key);
        compute_writeKey(children[1], key);
    }
    else if ( /* keep ? */keyLod < (parentLod + 1))
    {
        compute_writeKey(nodeID, key);
    }
    else /* merge ? */
    {
        if ( /* is root ? */ts_isRoot_64(nodeID))
        {
            compute_writeKey(nodeID, key);
        }
        else if ( /* is zero child ? */ts_isZeroChild_64(nodeID))
        {
            compute_writeKey(ts_parent_64(nodeID), key);
        }
    }
    
    cullPass(key);
}
