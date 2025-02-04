#pragma once

#include "RenderPasses.h"
#include "Camera.h"
#include "TessellationPSOData.h"
#include "AdaptiveTessellationCompute.h"

#include "../Core/Graphics/TextureD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace DirectX;

	struct Light;

	class CSMRendering
	{
	public:
		CSMRendering(RenderDeviceD3D12* device, TessellationPSOData* psoData);

		void Render(Camera* camera, Light* light, const Timer& timer, const AdaptiveTessellationCompute* adaptiveTessellation);

		int GetCascadeCount() const { return CascadeCount; }
		XMMATRIX GetCascadeTransform(int index) const { return m_CascadeTransforms[index]; }
		float GetCascadeDistance(int index) const { return CSMSplits[index] * ShadowDistance; }
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return m_ShadowMaps[0]->GetView(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGpuHandle(); }

		static constexpr int MaxCascades = 4;

	private:
		XMMATRIX CalculateLightView(Light* light);
		XMMATRIX CalculateCascadeProjection(float nearDist, float farDist, Camera* camera, XMMATRIX lightView);

	private:
		RenderDeviceD3D12* m_Device;
		TessellationPSOData* m_PsoData;

		XMMATRIX m_CascadeTransforms[MaxCascades];
		std::unique_ptr<TextureD3D12> m_ShadowMaps[MaxCascades];

		static constexpr int CascadeCount = 4;
		static constexpr float ShadowDistance = 200.0f;
		static constexpr XMFLOAT2 CSMSizes[4] =
		{
			{ 2048, 2048 },
			{ 1024, 1024 },
			{ 512,	512	 },
			{ 256,	256	 },
		};
		static constexpr float CSMSplits[4] = { 0.25f, 0.50f, 0.75f, 1.0f };
	};
}