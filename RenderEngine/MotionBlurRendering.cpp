#include "MotionBlurRendering.h"

#include "../Core/Graphics/DynamicUploadBuffer.h"

namespace AsyncComputeTessellation
{
	MotionBlurRendering::MotionBlurRendering(RenderDeviceD3D12* device, const SwapChain* swapChain, ScreenSpaceQuad* ssQuad) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_SSQuad(ssQuad)
	{
		D3D_SHADER_MACRO macros[] =
		{
			{"SAMPLE_COUNT", "20"},
			{NULL, NULL}
		};

		m_RenderPass = std::make_unique<MotionBlurPass>(m_Device, macros);
	}

	void MotionBlurRendering::Render(const Camera* camera, const GBuffer* gBuffer)
	{
		auto viewProj = XMMatrixMultiply(XMLoadFloat4x4(&camera->GetViewMatrix()), XMLoadFloat4x4(&camera->GetProjectionMatrix()));
		auto viewProjInv = XMMatrixInverse(nullptr, viewProj);
		auto prevViewProj = XMMatrixMultiply(XMLoadFloat4x4(&camera->GetPrevViewMatrix()), XMLoadFloat4x4(&camera->GetProjectionMatrix()));
		
		MotionBlurPass::PassData passData = {};
		XMStoreFloat4x4(&passData.ViewProjInv, XMMatrixTranspose(viewProjInv));
		XMStoreFloat4x4(&passData.PreviousViewProj, XMMatrixTranspose(prevViewProj));
		passData.BlurAmount = 5.0f;

		DynamicUploadBuffer passDataBuffer(m_Device, QueueID::Direct);
		passDataBuffer.LoadData(passData);

		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.SetRenderTargets(1, &(gBuffer->GetAccumBuffRTVView(1)), true, nullptr);

		commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineState());
		commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

		commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
		commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
		commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, gBuffer->GetAccumBuffSRVView(0));
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_SwapChain->DepthStencilSRVView());
		commandContext.GetCmdList()->SetGraphicsRootConstantBufferView(2, passDataBuffer.GetAllocation().GPUAddress);

		commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}
}