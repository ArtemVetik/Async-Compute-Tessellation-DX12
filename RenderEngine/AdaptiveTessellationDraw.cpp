#include "AdaptiveTessellationDraw.h"

namespace AsyncComputeTessellation
{
	AdaptiveTessellationDraw::AdaptiveTessellationDraw(RenderDeviceD3D12* device, SwapChain* swapChain, TessellationPSOData* psoData) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_PsoData(psoData)
	{
		m_DiffuseMap = std::make_unique<TextureD3D12>(m_Device, std::wstring(L"Textures/rock_boulder_dry_diff_4k.dds"), QueueID::Direct);
		
		auto desc = m_DiffuseMap->GetD3D12Resource()->GetDesc();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = desc.Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc.Texture2D.PlaneSlice = 0;

		m_DiffuseMap->CreateSRVView(&srvDesc, false);
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

#ifdef USE_STANDART_TESSELLATION
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(3, m_DiffuseMap->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());
#else
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, m_DiffuseMap->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());
#endif
	}
}