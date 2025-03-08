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

		D3D12_GPU_DESCRIPTOR_HANDLE GetBloomSrv() const { return m_BloomMipUp[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle(); }

		void SetThreshold(float value) { m_Threshold = value; }
		void SetIntensity(float value) { m_Intensity = value; }
		void SetScatter(float value) { m_Scatter = value; }
		void SetTint(const float* value) { memcpy(m_Tint, value, sizeof(float) * 3); }
		
		float GetThreshold() const { return m_Threshold; }
		float GetIntensity() const { return m_Intensity; }
		float GetScatter() const { return m_Scatter; }
		const float* GetTint() const { return m_Tint; }

	private:
		static constexpr int BloomMipCount = 6;
		int m_MipNum;

		RenderDeviceD3D12* m_Device;
		ScreenSpaceQuad* m_SSQuad;

		std::unique_ptr<BloomPass> m_RenderPass;

		std::unique_ptr<TextureD3D12> m_BloomMipDown[BloomMipCount];
		std::unique_ptr<TextureD3D12> m_BloomMipUp[BloomMipCount];

		float m_Threshold;
		float m_Intensity;
		float m_Scatter;
		float m_Tint[3];
	};
}