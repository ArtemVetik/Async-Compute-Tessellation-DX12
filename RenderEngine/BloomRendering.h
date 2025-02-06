#pragma once

#include "RenderPasses.h"
#include "ScreenSpaceQuad.h"

#include "../Core/Graphics/SwapChain.h"
#include "../Core/Graphics/GBuffer.h"

namespace AsyncComputeTessellation
{
	class BloomRendering
	{
	public:
		BloomRendering(RenderDeviceD3D12* device, ScreenSpaceQuad* ssQuad);

		void Resize(UINT w, UINT h);
		void Render(const GBuffer* gBuffer);
		void RenderImGui();

		D3D12_GPU_DESCRIPTOR_HANDLE GetBloomSrv() const { return m_BloomBuffer[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle(); };

	private:
		void BuildPSOAndWeights();

	private:
		RenderDeviceD3D12* m_Device;
		ScreenSpaceQuad* m_SSQuad;

		std::unique_ptr<BloomPass> m_RenderPass;

		std::unique_ptr<TextureD3D12> m_BloomBuffer[2];
		std::unique_ptr<BufferD3D12> m_WeightsBuffer;

		float m_Threshold;
		int m_KernelSize;
	};
}