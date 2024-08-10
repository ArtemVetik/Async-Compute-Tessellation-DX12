#pragma once

enum class CBVSRVUAVIndex
{
	IMGUI_TEXTURE = 0,
	MESH_DATA_VERTEX_UAV = 1,
	MESH_DATA_VERTEX_SRV = 2,
	MESH_DATA_INDEX_UAV = 3,
	MESH_DATA_INDEX_SRV = 4,
	DRAW_ARGS_UAV_0 = 5,
	DRAW_ARGS_UAV_1 = 6,
	SUBD_IN_UAV = 7,
	SUBD_OUT_UAV = 8,
	SUBD_OUT_CULL_UAV_0 = 9,
	SUBD_OUT_CULL_SRV_0 = 10,
	SUBD_OUT_CULL_UAV_1 = 12,
	SUBD_OUT_CULL_SRV_1 = 12,
	SUBD_COUNTER_UAV = 13,
	SHADOW_MAP_SRV = 14,
	G_BUFFER = 15, // 15 - 16 (2)
	ACCUMULATION_BUFFER = 17, // 17 - 18 (2)
	BLOOM_BUFFER = 19, // 19 - 20 (2)
	BLOOM_WEIGHTS = 21,
	DEPTH_BUFFER = 22,
};

enum class RTVIndex
{
	SWAP_CHAIN = 0, // 0 - 1 (2)
	G_BUFFER = 2, // 2 - 3 (2)
	ACCUMULATION_BUFFER = 4, // 4 - 5 (2)
	BLOOM_BUFFER = 6, // 6 - 7 (2)
};

enum class DSVIndex
{
	DEPTH_STENCIL_MAIN = 0,
	SHADOW_MAP_DEPTH = 1,
};