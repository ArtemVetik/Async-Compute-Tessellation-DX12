#include "DeferredLightRendering.h"
#include "GeometryGenerator.h"

#include <string>

#include "../Core/Graphics/DynamicUploadBuffer.h"
#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	DeferredLightRendering::DeferredLightRendering(RenderDeviceD3D12* device, const SwapChain* swapChain) :
		m_Device(device),
		m_SwapChain(swapChain)
	{
		m_DeferredLightPass = std::make_unique<DeferredLightPass>(m_Device);
		m_ToneMappingPass = std::make_unique<ToneMappingPass>(m_Device);

		GeometryGenerator geoGen;
		GeometryGenerator::MeshData quad = geoGen.CreateQuad(-1, 1, 2, 2, 0);

		m_QuadVertexBuff = std::make_unique<VertexBufferD3D12>(m_Device, quad.GetVerticesPT().data(),
			sizeof(VertexPT), (UINT)quad.GetVerticesPT().size());
		m_QuadIndexBuff = std::make_unique<IndexBufferD3D12>(m_Device, quad.GetIndices16().data(),
			sizeof(uint16_t), (UINT)quad.GetIndices16().size(), DXGI_FORMAT_R16_UINT);

		auto light0 = std::make_shared<Light>();
		light0->LightType = Light::Type::Directional;
		light0->Strength = { 1.0f, 1.0f, 1.0f };
		light0->Position = { 50.0f, 50.0f, 0.0f };
		light0->FalloffEnd = 50.0f;

		m_Lights.emplace_back(light0);
	}

	void DeferredLightRendering::RenderLights(const Camera* camera, const GBuffer* gBuffer)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.SetRenderTargets(1, &(gBuffer->GetAccumBuffRTVView()), true, nullptr);

		const float color[4] = { 0, 0, 0, 1 };
		commandContext.GetCmdList()->ClearRenderTargetView(m_SwapChain->CurrentBackBufferView(), color, 0, nullptr);

		commandContext.GetCmdList()->SetPipelineState(m_DeferredLightPass->GetD3D12PipelineState());
		commandContext.GetCmdList()->SetGraphicsRootSignature(m_DeferredLightPass->GetD3D12RootSignature());

		commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &(m_QuadVertexBuff->GetView()));
		commandContext.GetCmdList()->IASetIndexBuffer(&(m_QuadIndexBuff->GetView()));
		commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, gBuffer->GetGBufferSRVView(0));
		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(1, gBuffer->GetGBufferSRVView(1));
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
		lightPassConstants.FresnelR0 = { 0.01f, 0.01f, 0.01f };
		lightPassConstants.Roughness = 0.25f;

		for (size_t i = 0; i < m_Lights.size(); i++)
		{
			if (m_Lights[i]->LightType == Light::Type::Directional)
				lightPassConstants.DirectionalLightsCount++;
			if (m_Lights[i]->LightType == Light::Type::Point)
				lightPassConstants.PointLightsCount++;
			if (m_Lights[i]->LightType == Light::Type::Spotlight)
				lightPassConstants.SpotLightsCount++;
		}
		lightPassConstants.CascadeCount = 1;

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

		commandContext.GetCmdList()->SetGraphicsRootConstantBufferView(5, lightPassUploadBuffer.GetAllocation().GPUAddress);

		commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	void DeferredLightRendering::RenderToneMapping(const Camera* camera, const GBuffer* gBuffer)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.SetRenderTargets(1, &(m_SwapChain->CurrentBackBufferView()), true, &(m_SwapChain->DepthStencilView()));

		commandContext.GetCmdList()->SetPipelineState(m_ToneMappingPass->GetD3D12PipelineState());
		commandContext.GetCmdList()->SetGraphicsRootSignature(m_ToneMappingPass->GetD3D12RootSignature());

		commandContext.GetCmdList()->IASetVertexBuffers(0, 1, &(m_QuadVertexBuff->GetView()));
		commandContext.GetCmdList()->IASetIndexBuffer(&(m_QuadIndexBuff->GetView()));
		commandContext.GetCmdList()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandContext.GetCmdList()->SetGraphicsRootDescriptorTable(0, gBuffer->GetAccumBuffSRVView());

		commandContext.GetCmdList()->DrawIndexedInstanced(6, 1, 0, 0, 0);
	}

	void DeferredLightRendering::RednerImGui()
	{
		static bool showLight = false;

		if (ImGui::CollapsingHeader("Light parameters"))
			ImGui::Checkbox("Show Settings", &showLight);

		if (!showLight)
			return;

		ImGui::Begin("Lights Settings");

		if (ImGui::Button("Add Light"))
		{
			auto light = std::make_shared<Light>();
			light->LightType = Light::Type::Directional;
			light->Strength = { 0.8f, 0.8f, 0.8f };
			light->Position = { 0.0f, 50.0f, 0.0f };
			light->Direction = { 0.0f, -1.0f, 0.0f };
			light->FalloffEnd = 50.0f;

			m_Lights.emplace_back(light);
		}

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

			ImGui::Text("Light: %d\n", i);
			ImGui::Text("Position: (%f %f %f)", x, y, z);
			ImGui::Text("Direction: (%f %f %f)", m_Lights[i]->Direction.x, m_Lights[i]->Direction.y, m_Lights[i]->Direction.z);
			
			if (ImGui::SliderFloat((std::string("Radius##") + std::to_string(i)).c_str(), &radius, 0.1f, 300.0f))
				update = true;
			if (ImGui::SliderFloat((std::string("Theta##") + std::to_string(i)).c_str(), &theta, 0.0f, XM_PIDIV2))
				update = true;
			if (ImGui::SliderFloat((std::string("Phi##") + std::to_string(i)).c_str(), &phi, 0.0f, XM_2PI))
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
}