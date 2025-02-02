#pragma once
#include "Camera.h"
#include "Timer.h"
#include "RenderPasses.h"
#include "TessellationMesh.h"
#include "TessellationUI.h"

namespace AsyncComputeTessellation
{
	struct TessellationParams
	{
		MeshMode MeshMode = MeshMode::TERRAIN;
		bool WireframeMode = true;
		bool FlatNormals = false;
		int CPULodLevel = 0;
		bool Uniform = false;
		float TargetLength = 25;
		bool UseDisplaceMapping = true;

		TessellationComputePass::TessellationData CB;
	};

	class AdaptiveTessellation
	{
	public:
		AdaptiveTessellation(RenderDeviceD3D12* device, const Camera* camera);

		void Compute(const Timer& timer);
		void UpdateParams(UINT screenWidth, UINT screenHeight);

		friend class TessellationUI;

	private:
		void BuildPSO();
		void InitBuffers();
		void ResetBuffers();
		void UpdateLeafMesh();
		void InitTessData();

	private:
		RenderDeviceD3D12* m_Device;
		const Camera* m_Camera;
		TessellationUI m_UI;
		TessellationMesh m_Mesh;

		std::unique_ptr<TessellationComputePass> m_ComputePass;
		std::unique_ptr<TessellationDrawPass> m_DrawPass;

		std::unique_ptr<VertexBufferD3D12> m_LeafMeshVertex;
		std::unique_ptr<IndexBufferD3D12> m_LeafMeshIndex;

		std::unique_ptr<BufferD3D12> m_DrawArgs0;
		std::unique_ptr<BufferD3D12> m_DrawArgs1;
		std::unique_ptr<BufferD3D12> m_SubdBufferIn;
		std::unique_ptr<BufferD3D12> m_SubdBufferOut;
		std::unique_ptr<BufferD3D12> m_SubdBufferOutCulled0;
		std::unique_ptr<BufferD3D12> m_SubdBufferOutCulled1;
		std::unique_ptr<BufferD3D12> m_SubdCounter;
		std::unique_ptr<BufferD3D12> m_TessellationData;

		int m_PingPongCounter;
		
		TessellationParams m_Params;
		bool m_Freeze;
	};
}