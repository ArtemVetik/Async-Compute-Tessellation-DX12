#pragma once

#include "framework.h"
#include "Camera.h"
#include "Timer.h"
#include "RenderPasses.h"
#include "TessellationMesh.h"
#include "TessellationUI.h"
#include "TessellationPSOData.h"

#include "../Core/Graphics/GBuffer.h"
#include "../Core/Graphics/SwapChain.h"
#include "../Core/Graphics/DynamicUploadBuffer.h"

namespace AsyncComputeTessellation
{
	class AdaptiveTessellationCompute
	{
	public:
		AdaptiveTessellationCompute(RenderDeviceD3D12*   device,
									SwapChain*		     swapChain,
									const Camera*		 camera,
									TessellationPSOData* psoData,
									bool				 computeQueue);

		void Compute(const Timer& timer);
		void PrepareDraw();
		void RenderImGui();
		void ForceRebuildAll(bool computeQueue);

		void ExecuteIndirect() const;

		void SetParams(TessellationParams params);
		TessellationParams GetParams() const { return m_Params; }

		friend class TessellationUI;

	private:
#ifndef USE_STANDART_TESSELLATION
		struct VertexOut
		{
			XMFLOAT3 PosW;
			UINT Lvl;
			XMFLOAT3 NormalW;
			UINT Padding0;
			XMFLOAT2 TexC;
			XMFLOAT2 LeafPos;
		};
#endif

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
#ifdef USE_STANDART_TESSELLATION
		std::unique_ptr<BufferD3D12> m_SubdBufferOutCulled0;
		std::unique_ptr<BufferD3D12> m_SubdBufferOutCulled1;
#else
		std::unique_ptr<BufferD3D12> m_SubdBufferOutCulled;
		std::unique_ptr<BufferD3D12> m_VSPrepassOutV[2];
		std::unique_ptr<BufferD3D12> m_VSPrepassOutIdx[2];
#endif
		std::unique_ptr<BufferD3D12> m_SubdCounter;
		std::unique_ptr<BufferD3D12> m_TessellationData;

		int m_PingPongCounter;
		int m_SubdCulledBuffIdx;
		bool m_ComputeQueue;
		
		TessellationParams m_Params;

		DynamicUploadBuffer m_ObjectCB;
		DynamicUploadBuffer m_FrameCB;
	};
}