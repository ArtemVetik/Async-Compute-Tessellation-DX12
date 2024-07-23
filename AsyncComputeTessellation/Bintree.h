#pragma once

#include <vector>
#include "d3dUtil.h"
#include "GeometryGenerator.h"
#include "UploadBuffer.h"
#include "FrameResource.h"
#include "ImguiParams.h"

class Bintree
{
public:
	using uint32 = std::uint32_t;
	using uint16 = std::uint16_t;

	Bintree(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	void InitMesh(MeshMode mode);
	void UploadMeshData(ID3D12Resource* vertexResource, ID3D12Resource* indexResource);
	void UploadSubdivisionBuffer(ID3D12Resource* subdivisionBuffer);
	void UploadSubdivisionCounter(ID3D12Resource* subdivisionCounter);
	void UploadDrawArgs(ID3D12Resource* drawArgs, int cpuLodLevel);
	void UpdateLodFactor(ImguiParams* settings, int res, float fov);

	GeometryGenerator::MeshData GetMeshData() const;
private:
	ID3D12Device* mDevice;
	ID3D12GraphicsCommandList* mCommandList;

	GeometryGenerator::MeshData mMeshData;
	std::unique_ptr<MeshGeometry> mLeafGeometry;

	std::unique_ptr<UploadBuffer<Vertex>> MeshDataVertexUploadBuffer;
	std::unique_ptr<UploadBuffer<UINT>> MeshDataIndexUploadBuffer;
	std::unique_ptr<UploadBuffer<DirectX::XMUINT4>> SubdBufferInUploadBuffer;
	std::unique_ptr<UploadBuffer<IndirectCommand>> IndirectCommandUploadBuffer;
	std::unique_ptr<UploadBuffer<UINT>> SubdCounterUploadBuffer;

	MeshGeometry* BuildLeafMesh(uint32 cpuTessLevel);
	std::vector<DirectX::XMFLOAT3> GetLeafVertices(uint32 level);
	std::vector<uint16> GetLeafIndices(uint32 level);
};

