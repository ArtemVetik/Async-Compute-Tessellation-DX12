#pragma once

enum MeshMode
{
	TERRAIN = 0,
	MESH = 1,
};

struct ImguiParams
{
	bool ShowDebugWindow = false;
	bool WireframeMode = true;
	bool UseDisplaceMapping = true;
	MeshMode MeshMode = MeshMode::TERRAIN;
	int CPULodLevel = 0;
	int GPULodLevel = 0;
};