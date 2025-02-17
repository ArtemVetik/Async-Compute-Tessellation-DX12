#pragma once

#include "RenderPasses.h"
#include "TessellationMesh.h"

namespace AsyncComputeTessellation
{
	struct TessellationParams
	{
		MeshMode MeshMode = MeshMode::TERRAIN;
		bool WireframeMode = true;
		bool FlatNormals = false;
		int CPULodLevel = 0;
		bool Uniform = false;
		float TargetLength = 25;
		bool UseDisplaceMapping = true;
		bool Freeze = false;
		bool UseFP16InShader = false;

		TessellationComputePass::TessellationData CB;
	};

	class TessellationPSOData
	{
	private:
		RenderDeviceD3D12* m_Device;

		TessellationDrawRootSignature m_DrawRootSignature;
		std::unique_ptr<TessellationComputePass> m_ComputePass;
		std::unique_ptr<TessellationShadowMapPass> m_ShadowPass;
		std::unique_ptr<TessellationGBufferPass> m_DrawPass;

	public:
		TessellationPSOData(RenderDeviceD3D12* device) :
			m_Device(device),
			m_DrawRootSignature(device)
		{
		}

		void Rebuild(const TessellationParams& params)
		{
			const LPCWSTR macros[] =
			{
				L"USE_DISPLACE", params.UseDisplaceMapping && params.MeshMode == MeshMode::TERRAIN ? L"1" : L"0",
				L"UNIFORM_TESSELLATION", params.Uniform ? L"1" : L"0",
				L"FLAT_NORMALS", params.FlatNormals ? L"1" : L"0",
				L"USE_FP16", params.UseFP16InShader ? L"1" : L"0",
				NULL, NULL,
			};

			m_ComputePass = std::make_unique<TessellationComputePass>(m_Device, QueueID::Direct, macros);
			m_DrawPass = std::make_unique<TessellationGBufferPass>(m_Device, &m_DrawRootSignature, params.WireframeMode, macros);

			const LPCWSTR shadowMacros[] =
			{
				L"SHADOW_MAP", L"1",
				L"USE_DISPLACE", params.UseDisplaceMapping && params.MeshMode == MeshMode::TERRAIN ? L"1" : L"0",
				L"USE_FP16", params.UseFP16InShader ? L"1" : L"0",
				NULL, NULL,
			};
			m_ShadowPass = std::make_unique<TessellationShadowMapPass>(m_Device, &m_DrawRootSignature, shadowMacros);
		}

		const TessellationDrawRootSignature* GetDrawRootSignature() const { return &m_DrawRootSignature; }
		const TessellationComputePass* GetComputePass() const { return m_ComputePass.get(); }
		const TessellationShadowMapPass* GetShadowPass() const { return m_ShadowPass.get(); }
		const TessellationGBufferPass* GetGBufferPass() const { return m_DrawPass.get(); }
	};
}