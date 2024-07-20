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
	float TargetLength = 25;
	float LodFactor = 1;
	int CPULodLevel = 0;
	bool Uniform = false;
	int GPULodLevel = 0;
};

struct ImguiOutput
{
	bool RebuildMesh = false;
	bool ReuploadBuffers = false;
	bool RecompileShaders = false;

	bool HasChanges() const
	{
		return RebuildMesh || ReuploadBuffers || RecompileShaders;
	}
};