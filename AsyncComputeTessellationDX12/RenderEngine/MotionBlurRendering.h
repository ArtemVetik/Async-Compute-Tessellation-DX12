#pragma once

#include "ScreenSpaceQuad.h"
#include "RenderPasses.h"
#include "Camera.h"

#include "../Core/Graphics/SwapChain.h"
#include "../Core/Graphics/GBuffer.h"

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

	class MotionBlurRendering
	{
	public:
		MotionBlurRendering(RenderDeviceD3D12* device, const SwapChain* swapChain, ScreenSpaceQuad* ssQuad);

		void Render(const Camera* camera, const GBuffer* gBuffer, const Timer* timer);
		void RenderImGui();

		void SetSampleCount(int value);
		void SetBlurAmount(float value);

		int GetSampleCount() const { return m_SampleCount; }
		float GetBlurAmount() const { return m_BlurAmount; }

	private:
		void BuildPSO();

	private:
		RenderDeviceD3D12* m_Device;
		const SwapChain* m_SwapChain;
		ScreenSpaceQuad* m_SSQuad;

		std::unique_ptr<MotionBlurPass> m_RenderPass;

		int m_SampleCount;
		float m_BlurAmount;
	};
}