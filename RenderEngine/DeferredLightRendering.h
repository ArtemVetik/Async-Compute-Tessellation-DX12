#pragma once

#include <memory>

#include "RenderPasses.h"
#include "Camera.h"
#include "Timer.h"
#include "CSMRendering.h"

#include "../Core/Graphics/GBuffer.h"
#include "../Core/Graphics/BufferD3D12.h"
#include "../Core/Graphics/SwapChain.h"

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

	struct Light
	{
	public:
		enum Type
		{
			Directional = 0,
			Point = 1,
			Spotlight = 2
		};

		Type LightType = Type::Directional;
		DirectX::XMFLOAT3 Padding = { 0, 0, 0 };
		DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };
		float FalloffStart = 1.04f;							 // point/spot light only
		DirectX::XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f }; // directional/spot light only
		float FalloffEnd = 10.0f;							 // point/spot light only
		DirectX::XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };	 // point/spot light only
		float SpotPower = 64.0f;							 // spot light only
	};

	class DeferredLightRendering
	{
	public:
		DeferredLightRendering(RenderDeviceD3D12* device, const SwapChain* spawChain);

		void RenderLights(const Camera* camera, const CSMRendering* csmRendering);
		void RenderToneMapping(const Camera* camera);

		void RednerImGui(const Timer& timer);

		Light* GetShadowLight() const;
		GBuffer* GetGBuffer() const { return m_GBuffer.get(); }

	private:
		void AddDefaultLight();
		void InitMaterialBuffer();

	private:
		RenderDeviceD3D12* m_Device;
		const SwapChain* m_SwapChain;

		std::unique_ptr<GBuffer> m_GBuffer;
		std::unique_ptr<DeferredLightPass> m_DeferredLightPass;
		std::unique_ptr<ToneMappingPass> m_ToneMappingPass;

		std::unique_ptr<IndexBufferD3D12> m_QuadIndexBuff;
		std::unique_ptr<VertexBufferD3D12> m_QuadVertexBuff;
		std::unique_ptr<BufferD3D12> m_MaterialBuffer;

		std::vector<std::shared_ptr<Light>> m_Lights;
		DeferredLightPass::MaterialConstants m_MaterialData;
	};
}