#pragma once

#include "framework.h"
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

		std::unique_ptr<TextureD3D12> m_DiffuseMap;

		TessellationPSOData* m_PsoData;
	};
}