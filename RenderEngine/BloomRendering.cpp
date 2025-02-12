#include "BloomRendering.h"

#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	BloomRendering::BloomRendering(RenderDeviceD3D12* device, ScreenSpaceQuad* ssQuad) :
		m_Device(device),
		m_SSQuad(ssQuad),
		m_MipNum(BloomMipCount),
		m_Threshold(1.8f),
		m_Intensity(1.0f),
		m_Scatter(0.5f)
	{
		for (size_t i = 0; i < 3; i++)
			m_Tint[i] = 1.0f;

		m_RenderPass = std::make_unique<BloomPass>(m_Device, nullptr);
	}

	void BloomRendering::Resize(UINT w, UINT h)
	{
		for (size_t i = 0; i < BloomMipCount; i++)
		{
			m_BloomMipDown[i].reset();
			m_BloomMipUp[i].reset();
		}

		D3D12_RESOURCE_DESC resourceDesc;
		ZeroMemory(&resourceDesc, sizeof(resourceDesc));
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.MipLevels = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resourceDesc.Format = DeferredLightPass::AccumBuffFormat;

		D3D12_CLEAR_VALUE clearVal;
		clearVal.Color[0] = 0;
		clearVal.Color[1] = 0;
		clearVal.Color[2] = 0;
		clearVal.Color[3] = 1;
		clearVal.Format = DeferredLightPass::AccumBuffFormat;

		D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;
		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.Texture2D.MipLevels = 1;
		descSRV.Texture2D.MostDetailedMip = 0;
		descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		descSRV.Format = DeferredLightPass::AccumBuffFormat;

		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		m_MipNum = 0;
		for (int i = 0; i < BloomMipCount; i++)
		{
			resourceDesc.Width = w / (1 << (i + 1));
			resourceDesc.Height = h / (1 << (i + 1));

			if (resourceDesc.Width < 4 || resourceDesc.Height < 4)
				break;
			
			m_BloomMipDown[i] = std::make_unique<TextureD3D12>(m_Device, resourceDesc, &clearVal, QueueID::Direct);
			m_BloomMipUp[i] = std::make_unique<TextureD3D12>(m_Device, resourceDesc, &clearVal, QueueID::Direct);

			m_BloomMipDown[i]->CreateRTVView(nullptr, true);
			m_BloomMipDown[i]->CreateSRVView(&descSRV, false);

			m_BloomMipUp[i]->CreateRTVView(nullptr, true);
			m_BloomMipUp[i]->CreateSRVView(&descSRV, false);

			wchar_t bufferName[64];
			swprintf(bufferName, 64, L"BloomMipDown%d_%dx%d", i, (int)resourceDesc.Width, (int)resourceDesc.Height);
			m_BloomMipDown[i]->SetName(bufferName);

			swprintf(bufferName, 64, L"BloomMipUp%d_%dx%d", i, (int)resourceDesc.Width, (int)resourceDesc.Height);
			m_BloomMipUp[i]->SetName(bufferName);

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipDown[i]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipUp[i]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));

			m_MipNum++;
		}
	}

	void BloomRendering::Render(const GBuffer* gBuffer)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		D3D12_VIEWPORT viewport;
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;

		D3D12_RECT scissorRect = { 0, 0, 0, 0 };

		// bloom threshold pass
		{
			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipDown[0]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
			commandContext.FlushResourceBarriers();

			commandContext.SetRenderTargets(1, &(m_BloomMipDown[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);

			auto desc = m_BloomMipDown[0]->GetD3D12Resource()->GetDesc();
			viewport.Width = desc.Width;
			viewport.Height = desc.Height;
			scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext.SetViewports(&viewport, 1);
			commandContext.SetScissorRects(&scissorRect, 1);
			commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateThreshold());
			commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

			commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
			commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
			commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, gBuffer->GetAccumBuffSRVView(0));

			const float constants[8] = { m_Threshold, m_Intensity, m_Scatter, 0.0f, m_Tint[0], m_Tint[1], m_Tint[2], 0.0f };
			commandContext.GetCmdList()->SetGraphicsRoot32BitConstants(2, 8, constants, 0);

			commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipDown[0]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		}

		for (int i = 0; i < m_MipNum - 1; i++)
		{
			auto desc = m_BloomMipUp[i + 1]->GetD3D12Resource()->GetDesc();
			viewport.Width = desc.Width;
			viewport.Height = desc.Height;
			scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext.SetViewports(&viewport, 1);
			commandContext.SetScissorRects(&scissorRect, 1);

			// bloom H pass
			{
				commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipUp[i + 1]->GetD3D12Resource(),
					D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
				commandContext.FlushResourceBarriers();

				commandContext.SetRenderTargets(1, &(m_BloomMipUp[i + 1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);

				commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateH());
				commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

				commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
				commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
				commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_BloomMipDown[i]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());

				commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

				commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipUp[i + 1]->GetD3D12Resource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
			}

			// bloom V pass
			{
				commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipDown[i + 1]->GetD3D12Resource(),
					D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
				commandContext.FlushResourceBarriers();

				commandContext.SetRenderTargets(1, &(m_BloomMipDown[i + 1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);

				commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateV());
				commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

				commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
				commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
				commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_BloomMipUp[i + 1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());

				commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

				commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipDown[i + 1]->GetD3D12Resource(),
					D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
			}
		}

		// bloom upscaling pass
		for (int i = m_MipNum - 2; i >= 0; i--)
		{
			auto desc = m_BloomMipUp[i]->GetD3D12Resource()->GetDesc();
			viewport.Width = desc.Width;
			viewport.Height = desc.Height;
			scissorRect = { 0, 0, (int)viewport.Width, (int)viewport.Height };

			commandContext.SetViewports(&viewport, 1);
			commandContext.SetScissorRects(&scissorRect, 1);

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipUp[i]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
			commandContext.FlushResourceBarriers();

			commandContext.SetRenderTargets(1, &(m_BloomMipUp[i]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);

			commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateUpscale());
			commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

			commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
			commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
			commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, m_BloomMipDown[i]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());
			if (i == BloomMipCount - 2)
				commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_BloomMipDown[i + 1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());
			else
				commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_BloomMipUp[i + 1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());

			commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomMipUp[i]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
	}

	void BloomRendering::RenderImGui()
	{
		static bool open = false;

		if (!open)
		{
			if (ImGui::Button("Bloom"))
				open = true;
		}
		else
		{
			if (ImGui::Button("Bloom [X]"))
				open = false;
		}

		if (!open)
			return;

		if (ImGui::Begin("Bloom", &open))
		{
			ImGui::DragFloat("Threshold", &m_Threshold, 0.05f, 0.0f, FLT_MAX);
			ImGui::DragFloat("Intensity", &m_Intensity, 0.025f, 0.0f, FLT_MAX);
			ImGui::SliderFloat("Scatter", &m_Scatter, 0.0f, 1.0f);
			ImGui::ColorEdit3("Tint", m_Tint);
		}
		ImGui::End();
	}
}