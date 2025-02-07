#include "MotionBlurRendering.h"

#include "imgui/imgui.h"
#include "../Core/Graphics/DynamicUploadBuffer.h"

namespace AsyncComputeTessellation
{
	MotionBlurRendering::MotionBlurRendering(RenderDeviceD3D12* device, const SwapChain* swapChain, ScreenSpaceQuad* ssQuad) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_SSQuad(ssQuad),
		m_SampleCount(7),
		m_BlurAmount(8)
	{
		BuildPSO();
	}

	void MotionBlurRendering::Render(const Camera* camera, const GBuffer* gBuffer)
	{
		auto viewProj = XMMatrixMultiply(XMLoadFloat4x4(&camera->GetViewMatrix()), XMLoadFloat4x4(&camera->GetProjectionMatrix()));
		auto viewProjInv = XMMatrixInverse(nullptr, viewProj);
		auto prevViewProj = XMMatrixMultiply(XMLoadFloat4x4(&camera->GetPrevViewMatrix()), XMLoadFloat4x4(&camera->GetProjectionMatrix()));

		MotionBlurPass::PassData passData = {};
		XMStoreFloat4x4(&passData.ViewProjInv, XMMatrixTranspose(viewProjInv));
		XMStoreFloat4x4(&passData.PreviousViewProj, XMMatrixTranspose(prevViewProj));

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

		const float constants[4] = { m_BlurAmount, 0, 0, 0 };
		commandContext.GetCmdList()->SetGraphicsRoot32BitConstants(3, 4, constants, 0);

		commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	void MotionBlurRendering::RenderImGui()
	{
		static bool open = false;

		if (!open)
		{
			if (ImGui::Button("Motion Blur"))
				open = true;
		}
		else
		{
			if (ImGui::Button("Motion Blur [X]"))
				open = false;
		}

		if (!open)
			return;

		if (ImGui::Begin("Motion Blur", &open))
		{
			ImGui::SliderFloat("Blur Amount", &m_BlurAmount, 0.1f, 20.0f);

			if (ImGui::SliderInt("Sample Count", &m_SampleCount, 1, 50))
				BuildPSO();
		}
		ImGui::End();
	}

	void MotionBlurRendering::BuildPSO()
	{
		char sampleCountStr[4];
		sprintf_s(sampleCountStr, "%d", m_SampleCount);

		D3D_SHADER_MACRO macros[] =
		{
			{"SAMPLE_COUNT", sampleCountStr},
			{NULL, NULL}
		};

		m_RenderPass = std::make_unique<MotionBlurPass>(m_Device, macros);
	}
}