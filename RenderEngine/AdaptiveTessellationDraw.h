#pragma once

#include "Camera.h"
#include "Timer.h"
#include "RenderPasses.h"
#include "TessellationPSOData.h"

#include "../Core/Graphics/GBuffer.h"
#include "../Core/Graphics/SwapChain.h"
#include "../Core/Graphics/DynamicUploadBuffer.h"

namespace AsyncComputeTessellation
{
	class AdaptiveTessellationDraw
	{
	public:
		AdaptiveTessellationDraw(RenderDeviceD3D12* device, SwapChain* swapChain, TessellationPSOData* psoData);

		void Draw(const GBuffer* gBuffer);

	private:
		RenderDeviceD3D12* m_Device;
		SwapChain* m_SwapChain;

		TessellationPSOData* m_PsoData;
	};
}