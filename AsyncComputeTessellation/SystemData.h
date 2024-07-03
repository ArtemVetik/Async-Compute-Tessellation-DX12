#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <DirectXMath.h>
#include <d3d12.h>
#include "Vertex.h"
#include "wrl.h"

using namespace DirectX;

struct SubSystem
{
	uint32_t baseLocation;
	uint16_t count;

	SubSystem()
	{
		baseLocation = 0;
		count = 0;
	}
};

class SystemData
{
public:
	SystemData();
	~SystemData();

	const uint16_t GetCurrentBaseLocation();
	
	const uint16_t* GetIndices();

	const XMFLOAT3* GetPositions();
	const XMFLOAT3* GetNormals();
	const XMFLOAT2* GetUvs();

	SubSystem GetSubSystem(char* subSystemName) const;

	void LoadOBJFile(char* fileName, Microsoft::WRL::ComPtr<ID3D12Device> device, char* subSystemName);

private:
	uint16_t currentBaseLocation;

	uint16_t* indices;

	XMFLOAT3* positions;
	XMFLOAT3* normals;
	XMFLOAT2* uvs;

	std::unordered_map<char*, SubSystem> subSystemData;
};

