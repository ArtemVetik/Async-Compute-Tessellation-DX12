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
	bool WavesAnimation = false;
	MeshMode MeshMode = MeshMode::TERRAIN;
	float DisplaceFactor = 10.0f;
	float DisplaceLacunarity = 1.99;
	float DisplacePosScale = 0.02;
	float DisplaceH = 0.96;
	int CPULodLevel = 0;
	int GPULodLevel = 0;
};