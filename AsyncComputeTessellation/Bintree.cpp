#include "Bintree.h"

#include <stdexcept>
#include <corecrt_math_defines.h>

Bintree::Bintree(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	mDevice = device;
	mCommandList = commandList;
}

void Bintree::InitMesh(MeshMode mode)
{
	GeometryGenerator geoGen;

	if (mode == MeshMode::TERRAIN)
		mMeshData = geoGen.CreateGrid(250.0f, 250.0f, 2, 2);
	else
		mMeshData = geoGen.CreateSphere(100, 10, 10);

	mMeshData.InitAvgEdgeLength();
}

void Bintree::UploadMeshData(ID3D12Resource* vertexResource, ID3D12Resource* indexResource)
{
	// Mesh Data Vertex 
	{
		if (MeshDataVertexUploadBuffer)
			MeshDataVertexUploadBuffer.reset();

		MeshDataVertexUploadBuffer = std::make_unique<UploadBuffer<Vertex>>(mDevice, mMeshData.Vertices.size(), false);

		for (int i = 0; i < mMeshData.Vertices.size(); i++)
			MeshDataVertexUploadBuffer->CopyData(i, mMeshData.Vertices[i]);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
		mCommandList->CopyResource(vertexResource, MeshDataVertexUploadBuffer->Resource());
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
	}

	// Mesh Data Index 
	{
		if (MeshDataIndexUploadBuffer)
			MeshDataIndexUploadBuffer.reset();

		MeshDataIndexUploadBuffer = std::make_unique<UploadBuffer<UINT>>(mDevice, mMeshData.Indices32.size(), false);

		for (int i = 0; i < mMeshData.Indices32.size(); i++)
			MeshDataIndexUploadBuffer->CopyData(i, mMeshData.Indices32[i]);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexResource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
		mCommandList->CopyResource(indexResource, MeshDataIndexUploadBuffer->Resource());
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexResource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
	}
}

void Bintree::UploadSubdivisionBuffer(ID3D12Resource* subdivisionBuffer)
{
	if (SubdBufferInUploadBuffer)
		SubdBufferInUploadBuffer.reset();

	SubdBufferInUploadBuffer = std::make_unique<UploadBuffer<DirectX::XMUINT4>>(mDevice, mMeshData.Indices32.size() / 3, false);

	for (int i = 0; i < mMeshData.Indices32.size() / 3; i++)
		SubdBufferInUploadBuffer->CopyData(i, DirectX::XMUINT4(0, 0x1, i * 3, 1));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdivisionBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->CopyBufferRegion(subdivisionBuffer, 0, SubdBufferInUploadBuffer->Resource(), 0, mMeshData.Indices32.size() / 3 * sizeof(DirectX::XMUINT4));
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdivisionBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void Bintree::UploadSubdivisionCounter(ID3D12Resource* subdivisionCounter)
{
	if (SubdCounterUploadBuffer)
		SubdCounterUploadBuffer.reset();

	SubdCounterUploadBuffer = std::make_unique<UploadBuffer<UINT>>(mDevice, 2, false);

	SubdCounterUploadBuffer->CopyData(0, mMeshData.Indices32.size() / 3);
	SubdCounterUploadBuffer->CopyData(1, 0);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdivisionCounter, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->CopyResource(subdivisionCounter, SubdCounterUploadBuffer->Resource());
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdivisionCounter, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void Bintree::UploadDrawArgs(ID3D12Resource* drawArgs, int cpuLodLevel)
{
	MeshGeometry* mesh = BuildLeafMesh(cpuLodLevel);

	IndirectCommand command = {};
	command.VertexBufferView = mesh->VertexBufferView();
	command.IndexBufferView = mesh->IndexBufferView();
	command.DrawArguments.IndexCountPerInstance = mesh->IndexBufferByteSize / sizeof(uint16_t);
	command.DrawArguments.InstanceCount = 0;
	command.DrawArguments.StartIndexLocation = 0;
	command.DrawArguments.BaseVertexLocation = 0;
	command.DrawArguments.StartInstanceLocation = 0;

	if (IndirectCommandUploadBuffer)
		IndirectCommandUploadBuffer.reset();

	IndirectCommandUploadBuffer = std::make_unique<UploadBuffer<IndirectCommand>>(mDevice, 1, false);

	IndirectCommandUploadBuffer->CopyData(0, command);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(drawArgs, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->CopyResource(drawArgs, IndirectCommandUploadBuffer->Resource());
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(drawArgs, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void Bintree::UpdateLodFactor(ImguiParams* settings, int res, float fov)
{
	float l = 2.0f * tan(fov * (M_PI / 180) / 2.0f)
		* settings->TargetLength
		* (1 << settings->CPULodLevel)
		/ float(res);

	const float cap = 0.43f;
	if (l > cap) {
		l = cap;
	}
	settings->LodFactor = l / float(mMeshData.GetAvgEdgeLength());
}

GeometryGenerator::MeshData Bintree::GetMeshData() const
{
	return mMeshData;
}

MeshGeometry* Bintree::BuildLeafMesh(uint32 cpuTessLevel)
{
	auto vertices = GetLeafVertices(cpuTessLevel);
	auto indices = GetLeafIndices(cpuTessLevel);

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(DirectX::XMFLOAT3);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(uint16_t);

	if (mLeafGeometry == nullptr)
		mLeafGeometry = std::make_unique<MeshGeometry>();

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mLeafGeometry->VertexBufferCPU));
	CopyMemory(mLeafGeometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mLeafGeometry->IndexBufferCPU));
	CopyMemory(mLeafGeometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mLeafGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice,
		mCommandList, vertices.data(), vbByteSize, mLeafGeometry->VertexBufferUploader);

	mLeafGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(mDevice,
		mCommandList, indices.data(), ibByteSize, mLeafGeometry->IndexBufferUploader);

	mLeafGeometry->VertexByteStride = sizeof(DirectX::XMFLOAT3);
	mLeafGeometry->VertexBufferByteSize = vbByteSize;
	mLeafGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
	mLeafGeometry->IndexBufferByteSize = ibByteSize;

	return mLeafGeometry.get();
}

std::vector<DirectX::XMFLOAT3> Bintree::GetLeafVertices(uint32 level)
{
	std::vector<DirectX::XMFLOAT3> vertices;

	float num_row = 1 << level;
	float col = 0.0, row = 0.0;
	float d = 1.0 / float(num_row);

	while (row <= num_row)
	{
		while (col <= row)
		{
			vertices.push_back(DirectX::XMFLOAT3(col * d, 1.0 - row * d, 0));
			col++;
		}
		row++;
		col = 0;
	}

	return vertices;
}

std::vector<uint16_t> Bintree::GetLeafIndices(uint32 level)
{
	std::vector<uint16> indices;
	uint32 col = 0, row = 0;
	uint32 elem = 0, num_col = 1;
	uint32 orientation;
	uint32 num_row = 1 << level;
	auto new_triangle = [&]() {
		if (orientation == 0)
			return DirectX::XMINT3(elem, elem + num_col, elem + num_col + 1);
		else if (orientation == 1)
			return DirectX::XMINT3(elem, elem - 1, elem + num_col);
		else if (orientation == 2)
			return DirectX::XMINT3(elem, elem + num_col, elem + 1);
		else if (orientation == 3)
			return DirectX::XMINT3(elem, elem + num_col - 1, elem + num_col);
		else
			throw std::runtime_error("Bad orientation error");
		};
	while (row < num_row)
	{
		orientation = (row % 2 == 0) ? 0 : 2;
		while (col < num_col)
		{
			auto t = new_triangle();
			indices.push_back(t.x);
			indices.push_back(t.y);
			indices.push_back(t.z);
			orientation = (orientation + 1) % 4;
			if (col > 0) {
				auto t = new_triangle();
				indices.push_back(t.x);
				indices.push_back(t.y);
				indices.push_back(t.z);
				orientation = (orientation + 1) % 4;
			}
			col++;
			elem++;
		}
		col = 0;
		num_col++;
		row++;
	}
	return indices;
}
