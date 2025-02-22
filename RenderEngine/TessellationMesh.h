#pragma once

#include <memory>

#include "framework.h"
#include "GeometryGenerator.h"

#include "../Core/Graphics/BufferD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace DirectX;
	using namespace EduEngine;

	enum class MeshMode
	{
		TERRAIN, MESH
	};

	class TessellationMesh
	{
	private:
#ifdef USE_STANDART_TESSELLATION
		typedef uint16_t uint_idx;
#else
		typedef uint32_t uint_idx;
#endif

	public:
		TessellationMesh(RenderDeviceD3D12* device);

		void Initialize(MeshMode meshMode);

		GeometryGenerator::MeshData GetMeshData() const { return m_MeshData; }

		D3D12_GPU_DESCRIPTOR_HANDLE GetVertexUAVGpu() const { return m_MeshVertex->GetUAVView()->GetGpuHandle(); }
		D3D12_GPU_DESCRIPTOR_HANDLE GetVertexSRVGpu() const { return m_MeshVertex->GetSRVView()->GetGpuHandle(); }
		
		D3D12_GPU_DESCRIPTOR_HANDLE GetIndexUAVGpu() const { return m_MeshIndex->GetUAVView()->GetGpuHandle(); }
		D3D12_GPU_DESCRIPTOR_HANDLE GetIndexSRVGpu() const { return m_MeshIndex->GetSRVView()->GetGpuHandle(); }

		std::vector<XMFLOAT2> GetLeafVertices(uint32_t level);
		std::vector<uint_idx> GetLeafIndices(uint32_t level);

	private:
		RenderDeviceD3D12* m_Device;

		std::unique_ptr<VertexBufferD3D12> m_MeshVertex;
		std::unique_ptr<IndexBufferD3D12> m_MeshIndex;

		GeometryGenerator::MeshData m_MeshData;
	};
}