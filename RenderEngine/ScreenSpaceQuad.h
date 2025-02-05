#pragma once

#include "GeometryGenerator.h"

#include "../Core/Graphics/BufferD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

	class ScreenSpaceQuad
	{
	public:
		ScreenSpaceQuad(RenderDeviceD3D12* device)
		{
			GeometryGenerator geoGen;
			GeometryGenerator::MeshData quad = geoGen.CreateQuad(-1, 1, 2, 2, 0);

			m_QuadVertexBuff = std::make_unique<VertexBufferD3D12>(device, quad.GetVerticesPT().data(),
				sizeof(VertexPT), (UINT)quad.GetVerticesPT().size());
			m_QuadIndexBuff = std::make_unique<IndexBufferD3D12>(device, quad.GetIndices16().data(),
				sizeof(uint16_t), (UINT)quad.GetIndices16().size(), DXGI_FORMAT_R16_UINT);
		}

		D3D12_VERTEX_BUFFER_VIEW GetVertexView() const { return m_QuadVertexBuff->GetView(); }
		D3D12_INDEX_BUFFER_VIEW GetIndexView() const { return m_QuadIndexBuff->GetView(); }

	private:
		std::unique_ptr<IndexBufferD3D12> m_QuadIndexBuff;
		std::unique_ptr<VertexBufferD3D12> m_QuadVertexBuff;
	};
}