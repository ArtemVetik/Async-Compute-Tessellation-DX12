#include "BloomRendering.h"

#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	BloomRendering::BloomRendering(RenderDeviceD3D12* device, ScreenSpaceQuad* ssQuad) :
		m_Device(device),
		m_SSQuad(ssQuad),
		m_Threshold(1.8f),
		m_KernelSize(7)
	{
		BuildPSOAndWeights();
	}

	void BloomRendering::Resize(UINT w, UINT h)
	{
		for (size_t i = 0; i < 2; i++)
			m_BloomBuffer[i].reset();

		D3D12_RESOURCE_DESC resourceDesc;
		ZeroMemory(&resourceDesc, sizeof(resourceDesc));
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.MipLevels = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Width = w;
		resourceDesc.Height = h;
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

		for (size_t i = 0; i < 2; i++)
		{
			m_BloomBuffer[i] = std::make_unique<TextureD3D12>(m_Device, resourceDesc, &clearVal, QueueID::Direct);
			m_BloomBuffer[i]->CreateRTVView(nullptr, true);
			m_BloomBuffer[i]->CreateSRVView(&descSRV, false);

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[i]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ));
		}
	}

	void BloomRendering::Render(const GBuffer* gBuffer)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[0]->GetD3D12Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[1]->GetD3D12Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		commandContext.FlushResourceBarriers();

		// bloom threshold pass
		{
			commandContext.SetRenderTargets(1, &(m_BloomBuffer[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);

			commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateThreshold());
			commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

			commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
			commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
			commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, gBuffer->GetAccumBuffSRVView(0));
			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(2, m_WeightsBuffer->GetSRVView()->GetGpuHandle());

			const float constants[4] = { m_Threshold, 0.0f, 0.0f, 0.0f };
			commandContext.GetCmdList()->SetGraphicsRoot32BitConstants(3, 4, constants, 0);

			commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

			commandContext.GetCmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[0]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		}

		// TODO: make downsampling and upsampling
		// bloom H pass
		{
			commandContext.SetRenderTargets(1, &(m_BloomBuffer[1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);

			commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateH());
			commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

			commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
			commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
			commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_BloomBuffer[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());

			commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
		}

		// bloom V pass
		{
			commandContext.GetCmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[0]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
			commandContext.GetCmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[1]->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

			commandContext.SetRenderTargets(1, &(m_BloomBuffer[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->GetCpuHandle()), true, nullptr);


			commandContext.GetCmdList()->SetPipelineState(m_RenderPass->GetD3D12PipelineStateV());
			commandContext.GetCmdList()->SetGraphicsRootSignature(m_RenderPass->GetD3D12RootSignature());

			commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &m_SSQuad->GetVertexView());
			commandContext.GetCmdList()->IASetIndexBuffer(&m_SSQuad->GetIndexView());
			commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_BloomBuffer[1]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle());

			commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_BloomBuffer[0]->GetD3D12Resource(),
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
			ImGui::SliderFloat("Threshold", &m_Threshold, 0.0f, 3.0f);

			if (ImGui::SliderInt("Kernel Size", &m_KernelSize, 3, 32))
				BuildPSOAndWeights();

		}
		ImGui::End();
	}

	void BloomRendering::BuildPSOAndWeights()
	{
		char kernelSizeStr[4];
		sprintf_s(kernelSizeStr, "%d", m_KernelSize);

		D3D_SHADER_MACRO hMacros[] =
		{
			{"HORIZONTAL_BLUR", "1"},
			{"BLOOM_KERNEL_SIZE", kernelSizeStr},
			{NULL, NULL}
		};

		D3D_SHADER_MACRO vMacros[] =
		{
			{"HORIZONTAL_BLUR", "0"},
			{"BLOOM_KERNEL_SIZE", kernelSizeStr},
			{NULL, NULL}
		};

		m_RenderPass = std::make_unique<BloomPass>(m_Device, hMacros, vMacros);

		std::vector<float> weights;

		float sigma = 3.0f;
		float sum = 0.0f;
		for (int i = 0; i <= m_KernelSize; ++i) {
			float weight = expf(-0.5f * (i * i) / (sigma * sigma));
			weights.push_back(weight);
			sum += weight;
		}

		for (int i = 0; i < weights.size(); ++i)
			weights[i] /= sum;

		m_WeightsBuffer = std::make_unique<BufferD3D12>(m_Device, CD3DX12_RESOURCE_DESC::Buffer(sizeof(float) * weights.size()), weights.data(), QueueID::Direct);

		D3D12_SHADER_RESOURCE_VIEW_DESC bloomWeightsSRVDescription = {};
		bloomWeightsSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		bloomWeightsSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		bloomWeightsSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		bloomWeightsSRVDescription.Buffer.FirstElement = 0;
		bloomWeightsSRVDescription.Buffer.NumElements = m_KernelSize;
		bloomWeightsSRVDescription.Buffer.StructureByteStride = sizeof(float);

		m_WeightsBuffer->CreateSRV(&bloomWeightsSRVDescription);
	}
}