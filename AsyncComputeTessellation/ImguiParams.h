#pragma once

enum class RenderType
{
	Direct = 0,
	AsyncAll = 1,
	AsyncShadowMap = 2,
	AsyncPostProcess = 3,
};

enum MeshMode
{
	TERRAIN = 0,
	MESH = 1,
};

struct ImguiParams
{
	bool ShowDebugWindow = false;
	RenderType RenderType = RenderType::Direct;

	// Tessellation Parameters / View Mode
	MeshMode MeshMode = MeshMode::TERRAIN;
	bool WireframeMode = true;
	bool FlatNormals = false;

	// Tessellation Parameters / LoD
	int CPULodLevel = 0;
	bool Uniform = false;
	int GPULodLevel = 0;
	float LodFactor = 1;
	float TargetLength = 25;

	// Tessellation Parameters / Displace
	bool UseDisplaceMapping = true;
	bool WavesAnimation = false;
	float DisplaceFactor = 10.0f;
	float DisplaceLacunarity = 1.99;
	float DisplacePosScale = 0.02;
	float DisplaceH = 0.96;
	
	// Tessellation Parameters / Compute Settings
	bool Freeze = false;
	
	// Lighting / Directional Light
	int DirectionalLightCount = 3;
	float DLColor[3][3] = {
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
	};
	float DLIntensivity[3] = { 1, 1, 1 };
	float LightRotateSpeed = 1.0f;

	// Lighting / Shading Parameters
	float DiffuseAlbedo[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
	float AmbientLight[4] = { 0.55f, 0.55f, 0.55f, 1.0f };
	float Roughness = 0.125f;
	float FresnelR0[3] = {0.02f, 0.02f, 0.02f};

	// Motion Blur
	float MotionBlurAmount = 50.0f;
	int MotionBlurSamplerCount = 20;

	// Bloom
	float Threshold = 1.0f;
	int BloomKernelSize = 7;
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