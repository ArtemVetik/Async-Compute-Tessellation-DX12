#include "TessellationMesh.h"
#include "AdaptiveTessellation.h"

namespace AsyncComputeTessellation
{
	TessellationMesh::TessellationMesh(RenderDeviceD3D12* device) :
		m_Device(device)
	{
	}

	void TessellationMesh::Initialize(MeshMode meshMode)
	{
		GeometryGenerator geoGen;

		if (meshMode == MeshMode::TERRAIN)
			m_MeshData = geoGen.CreateGrid(250.0f, 250.0f, 2, 2);
		else
			m_MeshData = geoGen.LoadMesh("Models/Teapot.fbx");

		m_MeshVertex = std::make_unique<VertexBufferD3D12>(m_Device, m_MeshData.Vertices.data(), sizeof(Vertex), m_MeshData.Vertices.size(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		m_MeshVertex->SetName(L"MeshVertexBuffer");
		m_MeshIndex = std::make_unique<IndexBufferD3D12>(m_Device, m_MeshData.Indices32.data(), sizeof(UINT), m_MeshData.Indices32.size(), DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		m_MeshIndex->SetName(L"MeshIndexBuffer");

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = m_MeshData.Vertices.size();
		uavDesc.Buffer.StructureByteStride = sizeof(Vertex);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = m_MeshData.Vertices.size();
		srvDesc.Buffer.StructureByteStride = sizeof(Vertex);

		m_MeshVertex->CreateUAV(&uavDesc);
		m_MeshVertex->CreateSRV(&srvDesc);

		uavDesc.Buffer.NumElements = m_MeshData.Indices32.size();
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		srvDesc.Buffer.NumElements = m_MeshData.Indices32.size();
		srvDesc.Buffer.StructureByteStride = sizeof(UINT);

		m_MeshIndex->CreateUAV(&uavDesc);
		m_MeshIndex->CreateSRV(&srvDesc);

		m_MeshData.InitAvgEdgeLength();
	}

	std::vector<XMFLOAT3> TessellationMesh::GetLeafVertices(uint32_t level)
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

	std::vector<uint16_t> TessellationMesh::GetLeafIndices(uint32_t level)
	{
		std::vector<uint16_t> indices;
		uint32_t col = 0, row = 0;
		uint32_t elem = 0, num_col = 1;
		uint32_t orientation;
		uint32_t num_row = 1 << level;
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
}