#pragma once

#include <vector>
#include "d3dUtil.h"


class Bintree
{
public:
	using uint32 = std::uint32_t;
	using uint16 = std::uint16_t;

	Bintree(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	MeshGeometry* BuildLeafMesh(uint32 cpuTessLevel);
private:
	std::unique_ptr<MeshGeometry> mLeafGeometry;
	ID3D12Device* mDevice;
	ID3D12GraphicsCommandList* mCommandList;

	std::vector<DirectX::XMFLOAT3> GetLeafVertices(uint32 level);
	std::vector<uint16> GetLeafIndices(uint32 level);
};

