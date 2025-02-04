#pragma once

#include "Camera.h"
#include "Timer.h"
#include "RenderPasses.h"
#include "TessellationMesh.h"
#include "TessellationUI.h"
#include "TessellationPSOData.h"

#include "../Core/Graphics/GBuffer.h"
#include "../Core/Graphics/SwapChain.h"

namespace AsyncComputeTessellation
{
	class AdaptiveTessellationCompute
	{
	public:
		AdaptiveTessellationCompute(RenderDeviceD3D12*   device,
							 SwapChain*		      swapChain,
							 const Camera*		  camera,
							 TessellationPSOData* psoData);

		void Compute(const Timer& timer);
		void RenderImGui();

		void ExecuteIndirect() const;

		friend class TessellationUI;

	private:
		void BuildPSO();
		void InitBuffers();
		void ResetBuffers();
		void UpdateLeafMesh();
		void InitTessData();

	private:
		RenderDeviceD3D12* m_Device;
		SwapChain* m_SwapChain;
		const Camera* m_Camera;

		TessellationUI m_UI;
		TessellationMesh m_Mesh;

		TessellationPSOData* m_PsoData;

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
	};
}