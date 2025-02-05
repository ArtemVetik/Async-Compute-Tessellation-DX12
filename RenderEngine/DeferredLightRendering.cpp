#include "DeferredLightRendering.h"
#include "GeometryGenerator.h"

#include <string>

#include "../Core/Graphics/DynamicUploadBuffer.h"
#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	DeferredLightRendering::DeferredLightRendering(RenderDeviceD3D12* device, const SwapChain* swapChain, ScreenSpaceQuad* ssQuad) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_SSQuad(ssQuad),
		m_MaterialData{}
	{
		m_DeferredLightPass = std::make_unique<DeferredLightPass>(m_Device);
		m_ToneMappingPass = std::make_unique<ToneMappingPass>(m_Device);

		m_GBuffer = std::make_unique<GBuffer>(TessellationGBufferPass::GBufferCount, TessellationGBufferPass::RtvFormats, 2, DeferredLightPass::AccumBuffFormat);
		m_GBuffer->Resize(m_Device, m_SwapChain->GetWidth(), m_SwapChain->GetHeight());

		InitMaterialBuffer();
		AddDefaultLight();
	}

	void DeferredLightRendering::RenderLights(const Camera* camera, const CSMRendering* csmRendering)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		for (int i = 0; i < TessellationGBufferPass::GBufferCount; i++)
			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->GetGBuffer(i),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->GetDepthStencilBuffer(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
		commandContext.FlushResourceBarriers();

		commandContext.SetRenderTargets(1, &(m_GBuffer->GetAccumBuffRTVView(0)), true, nullptr);

		commandContext.GetCmdList()->SetPipelineState(m_DeferredLightPass->GetD3D12PipelineState());
		commandContext.GetCmdList()->SetGraphicsRootSignature(m_DeferredLightPass->GetD3D12RootSignature());

		commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &(m_SSQuad->GetVertexView()));
		commandContext.GetCmdList()->IASetIndexBuffer(&(m_SSQuad->GetIndexView()));
		commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, m_GBuffer->GetGBufferSRVView(0));
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, m_GBuffer->GetGBufferSRVView(1));
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(2, m_SwapChain->DepthStencilSRVView());

		DeferredLightPass::PassConstants lightPassConstants = {};
		auto projInv = XMMatrixInverse(nullptr, (XMLoadFloat4x4(&camera->GetProjectionMatrix())));
		auto viewInv = XMMatrixInverse(nullptr, (XMLoadFloat4x4(&camera->GetViewMatrix())));

		XMStoreFloat4x4(&lightPassConstants.ProjInv, projInv);
		XMStoreFloat4x4(&lightPassConstants.ViewInv, viewInv);
		XMStoreFloat4x4(&lightPassConstants.View, XMMatrixTranspose(XMLoadFloat4x4(&camera->GetViewMatrix())));

		lightPassConstants.EyePosW = camera->GetPosition();
		lightPassConstants.ClearColor = { 0, 0, 0, 1 };
		lightPassConstants.AmbientLight = { 1, 1, 1, 1 };

		for (size_t i = 0; i < m_Lights.size(); i++)
		{
			if (m_Lights[i]->LightType == Light::Type::Directional)
				lightPassConstants.DirectionalLightsCount++;
			if (m_Lights[i]->LightType == Light::Type::Point)
				lightPassConstants.PointLightsCount++;
			if (m_Lights[i]->LightType == Light::Type::Spotlight)
				lightPassConstants.SpotLightsCount++;
		}
		lightPassConstants.CascadeCount = csmRendering->GetCascadeCount();

		for (int i = 0; i < csmRendering->GetCascadeCount(); i++)
		{
			XMStoreFloat4x4(lightPassConstants.CascadeTransform + i, XMMatrixTranspose(csmRendering->GetCascadeTransform(i)));
			lightPassConstants.CascadeDistance[i] = csmRendering->GetCascadeDistance(i);
		}

		DynamicUploadBuffer lightPassUploadBuffer(m_Device, QueueID::Direct);
		lightPassUploadBuffer.LoadData(lightPassConstants);

		if (m_Lights.size() > 0)
		{
			DynamicUploadBuffer lightsUploadBuffer(m_Device, QueueID::Direct);
			lightsUploadBuffer.CreateAllocation(m_Lights.size() * sizeof(Light));

			int dirIdx = 0;
			int pointIdx = 0;
			int spotIdx = 0;

			for (size_t i = 0; i < m_Lights.size(); i++)
			{
				if (m_Lights[i]->LightType == Light::Type::Directional)
					lightsUploadBuffer.PutData(dirIdx++, *m_Lights[i].get());
				if (m_Lights[i]->LightType == Light::Type::Point)
					lightsUploadBuffer.PutData(lightPassConstants.DirectionalLightsCount + pointIdx++, *m_Lights[i].get());
				if (m_Lights[i]->LightType == Light::Type::Spotlight)
					lightsUploadBuffer.PutData(lightPassConstants.DirectionalLightsCount + lightPassConstants.PointLightsCount + spotIdx++, *m_Lights[i].get());
			}

			lightsUploadBuffer.CreateSRV(m_Lights.size(), sizeof(Light));
			commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(3, lightsUploadBuffer.GetSRVDescriptorGPUHandle());
		}

		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(4, csmRendering->GetGPUHandle());
		commandContext.GetCmdList()->SetGraphicsRootConstantBufferView(5, lightPassUploadBuffer.GetAllocation().GPUAddress);
		commandContext.GetCmdList()->SetGraphicsRootConstantBufferView(6, m_MaterialBuffer->GetD3D12Resource()->GetGPUVirtualAddress());

		commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	void DeferredLightRendering::RenderToneMapping(const Camera* camera, const BloomRendering* bloom)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->GetAccumBuffer(0),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->GetAccumBuffer(1),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		commandContext.FlushResourceBarriers();

		commandContext.SetRenderTargets(1, &(m_SwapChain->CurrentBackBufferView()), true, nullptr);

		commandContext.GetCmdList()->SetPipelineState(m_ToneMappingPass->GetD3D12PipelineState());
		commandContext.GetCmdList()->SetGraphicsRootSignature(m_ToneMappingPass->GetD3D12RootSignature());

		commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &(m_SSQuad->GetVertexView()));
		commandContext.GetCmdList()->IASetIndexBuffer(&(m_SSQuad->GetIndexView()));
		commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, m_GBuffer->GetAccumBuffSRVView(1));
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, bloom->GetBloomSrv());

		commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);

		for (int i = 0; i < TessellationGBufferPass::GBufferCount; i++)
			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->GetGBuffer(i),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->GetAccumBuffer(0),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_GBuffer->GetAccumBuffer(1),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->GetDepthStencilBuffer(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		commandContext.FlushResourceBarriers(); // TODO: maybe it's not necessary?
	}

	void DeferredLightRendering::RenderImGui(const Timer& timer)
	{
		static bool showLightMenu = false;
		static float rotateLightsSpeed = 0.0f;

		if (ImGui::CollapsingHeader("Light parameters"))
		{
			ImGui::Checkbox("Show Light Settings", &showLightMenu);
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::ColorEdit4("Diffuse Albedo", reinterpret_cast<float*>(&m_MaterialData.DiffuseAlbedo)))
				InitMaterialBuffer();
			if (ImGui::SliderFloat3("Fresnel R0", reinterpret_cast<float*>(&m_MaterialData.FresnelR0), 0.02f, 0.999f))
				InitMaterialBuffer();
			if (ImGui::SliderFloat("Roughness", &m_MaterialData.Roughness, 0.0f, 0.999f))
				InitMaterialBuffer();
		}

		if (!showLightMenu)
			return;

		ImGui::Begin("Lights Settings");

		ImGui::InputFloat("Rotate Speed", &rotateLightsSpeed);

		if (ImGui::Button("Add Light"))
			AddDefaultLight();

		for (size_t i = 0; i < m_Lights.size(); i++)
		{
			float x = m_Lights[i]->Position.x;
			float y = m_Lights[i]->Position.y;
			float z = m_Lights[i]->Position.z;

			float radius = sqrtf(x * x + y * y + z * z);
			float theta, phi;

			if (radius > 0.0f)
			{
				theta = acosf(y / radius);
				phi = atan2f(z, x);
			}
			else
			{
				theta = 0.0f;
				phi = 0.0f;
			}

			bool update = false;

			if (rotateLightsSpeed)
			{
				phi += rotateLightsSpeed * timer.GetDeltaTime();
				update = true;
			}

			ImGui::Text("Light: %d\n", i);
			ImGui::Text("Position: (%f %f %f)", x, y, z);
			ImGui::Text("Direction: (%f %f %f)", m_Lights[i]->Direction.x, m_Lights[i]->Direction.y, m_Lights[i]->Direction.z);

			if (ImGui::SliderFloat((std::string("Radius##") + std::to_string(i)).c_str(), &radius, 0.1f, 300.0f))
				update = true;
			if (ImGui::SliderFloat((std::string("Theta##") + std::to_string(i)).c_str(), &theta, 0.0f, XM_PIDIV2))
				update = true;
			if (ImGui::SliderFloat((std::string("Phi##") + std::to_string(i)).c_str(), &phi, -XM_PI, XM_PI))
				update = true;

			auto lightType = m_Lights[i]->LightType;
			if (ImGui::Combo((std::string("Light Type##") + std::to_string(i)).c_str(), (int*)&lightType, "Directional\0Point\0Spotlight\0\0"))
				m_Lights[i]->LightType = lightType;

			float strength[3] = { m_Lights[i]->Strength.x,m_Lights[i]->Strength.y, m_Lights[i]->Strength.z };
			if (ImGui::InputFloat3((std::string("Strength##") + std::to_string(i)).c_str(), strength))
				m_Lights[i]->Strength = { strength[0], strength[1], strength[2] };

			float falloffStart = m_Lights[i]->FalloffStart;
			if (ImGui::InputFloat((std::string("Falloff Start##") + std::to_string(i)).c_str(), &falloffStart))
				m_Lights[i]->FalloffStart = falloffStart;

			float falloffEnd = m_Lights[i]->FalloffEnd;
			if (ImGui::InputFloat((std::string("Falloff End##") + std::to_string(i)).c_str(), &falloffEnd))
				m_Lights[i]->FalloffEnd = falloffEnd;

			float spotPower = m_Lights[i]->SpotPower;
			if (ImGui::InputFloat((std::string("Spot Power##") + std::to_string(i)).c_str(), &spotPower))
				m_Lights[i]->SpotPower = spotPower;

			if (update)
			{
				float x = radius * sinf(theta) * cosf(phi);
				float y = radius * cosf(theta);
				float z = radius * sinf(theta) * sinf(phi);

				m_Lights[i]->Position = XMFLOAT3(x, y, z);
			}

			XMVECTOR direction = XMVectorSubtract({ 0, 0, 0 }, XMLoadFloat3(&m_Lights[i]->Position));
			XMFLOAT3 normDirection;
			XMStoreFloat3(&normDirection, XMVector3Normalize(direction));

			m_Lights[i]->Direction = normDirection;

			if (ImGui::Button((std::string("Remove Light##") + std::to_string(i)).c_str()))
				m_Lights.erase(m_Lights.begin() + i);

			ImGui::Separator();
		}

		ImGui::End();
	}

	Light* DeferredLightRendering::GetShadowLight() const
	{
		for (size_t i = 0; i < m_Lights.size(); i++)
			if (m_Lights[i]->LightType == Light::Directional)
				return m_Lights[i].get();

		return nullptr;
	}

	void DeferredLightRendering::AddDefaultLight()
	{
		auto light = std::make_shared<Light>();
		light->LightType = Light::Type::Directional;
		light->Strength = { 1.0f, 1.0f, 1.0f };
		light->Position = { 50.0f, 50.0f, 0.0f };
		light->Direction = { -0.707f, -0.707f, 0.0f };
		light->FalloffEnd = 50.0f;

		m_Lights.emplace_back(light);
	}

	void DeferredLightRendering::InitMaterialBuffer()
	{
		m_MaterialBuffer = std::make_unique<BufferD3D12>(
			m_Device,
			CD3DX12_RESOURCE_DESC::Buffer(sizeof(DeferredLightPass::MaterialConstants)),
			&m_MaterialData,
			QueueID::Direct
		);
	}
}