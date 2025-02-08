#include "AdaptiveTessellationDraw.h"

namespace AsyncComputeTessellation
{
	AdaptiveTessellationDraw::AdaptiveTessellationDraw(RenderDeviceD3D12* device, SwapChain* swapChain, TessellationPSOData* psoData) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_PsoData(psoData)
	{
		
	}

	void AdaptiveTessellationDraw::Draw(const GBuffer* gBuffer)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		D3D12_CPU_DESCRIPTOR_HANDLE gBuffViews[TessellationGBufferPass::GBufferCount];
		for (int i = 0; i < TessellationGBufferPass::GBufferCount; i++)
			gBuffViews[i] = gBuffer->GetGBufferRTVView(i);

		commandContext.SetRenderTargets(TessellationGBufferPass::GBufferCount, gBuffViews, false, &(m_SwapChain->DepthStencilView()));
		commandContext.GetCmdList()->ClearDepthStencilView(m_SwapChain->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		for (int i = 0; i < TessellationGBufferPass::GBufferCount; i++)
			commandContext.GetCmdList()->ClearRenderTargetView(gBuffer->GetGBufferRTVView(i), DirectX::Colors::Black, 0, nullptr);

		commandContext.GetCmdList()->SetPipelineState(m_PsoData->GetGBufferPass()->GetD3D12PipelineState());

		//TODO: add diffuse map
		//commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(3, diffuseMapSrv);
	}
}