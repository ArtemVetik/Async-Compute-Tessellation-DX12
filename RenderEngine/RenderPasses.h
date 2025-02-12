#pragma once

#include <vector>

#include "../Core/EduMath/SimpleMath.h"
#include "../Core/Graphics/PipelineStateD3D12.h"
#include "../Core/Graphics/ComputePipelineStateD3D12.h"
#include "../Core/Graphics/CommandSignatureD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace EduEngine;
	using namespace DirectX;

	class TessellationComputePass
	{
	public:
		struct ObjectData
		{
			XMFLOAT4X4 World = DirectX::SimpleMath::Matrix::Identity;
			XMFLOAT4X4 TexTransform = DirectX::SimpleMath::Matrix::Identity;
		};

		struct TessellationData
		{
			UINT SubdivisionLevel = 5;
			UINT ScreenRes;
			float DisplaceFactor = 10.0f;
			UINT WavesAnimationFlag = false;
			float DisplaceLacunarity = 1.99;
			float DisplacePosScale = 0.02;
			float DisplaceH = 0.96;
			float LodFactor = 0.00008f;
		};

		struct PerFrameData
		{
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CamPosition;
			float DeltaTime;
			XMFLOAT3 PredictedCamPosition;
			float TotalTime;
			XMFLOAT4 FrustrumPlanes[6];
		};

		struct IndirectCommand
		{
			D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
			D3D12_INDEX_BUFFER_VIEW IndexBufferView;
			D3D12_DRAW_INDEXED_ARGUMENTS DrawArguments;
		};

	private:
		ShaderD3D12 m_UpdateCS;
		ShaderD3D12 m_CopyDrawCS;
		RootSignatureD3D12 m_RootSignature;
		CommandSignatureD3D12 m_CommandSignature;
		ComputePipelineStateD3D12 m_UpdatePSO;
		ComputePipelineStateD3D12 m_CopyDrawPSO;

	public:
		TessellationComputePass(RenderDeviceD3D12* device, QueueID queueId, D3D_SHADER_MACRO* macros = nullptr) :
			m_UpdateCS(L"Shaders\\TessellationUpdate.hlsl", EDU_SHADER_TYPE_COMPUTE, macros, "main", "cs_5_1"),
			m_CopyDrawCS(L"Shaders\\TessellationCopyDraw.hlsl", EDU_SHADER_TYPE_COMPUTE, macros, "main", "cs_5_1")
		{
			// TODO: Order from most frequent to least frequent

			CD3DX12_DESCRIPTOR_RANGE subdBufferIn;
			subdBufferIn.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
			m_RootSignature.AddDescriptorParameter(1, &subdBufferIn); // subd buffer in

			CD3DX12_DESCRIPTOR_RANGE subdBufferOut;
			subdBufferOut.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
			m_RootSignature.AddDescriptorParameter(1, &subdBufferOut); // subd buffer out

			CD3DX12_DESCRIPTOR_RANGE subdBufferOutCulled;
			subdBufferOutCulled.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
			m_RootSignature.AddDescriptorParameter(1, &subdBufferOutCulled); // subd buffer out culled

			CD3DX12_DESCRIPTOR_RANGE meshDataVertex;
			meshDataVertex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);
			m_RootSignature.AddDescriptorParameter(1, &meshDataVertex); // mesh data vertex

			CD3DX12_DESCRIPTOR_RANGE meshDataIndex;
			meshDataIndex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);
			m_RootSignature.AddDescriptorParameter(1, &meshDataIndex); // mesh data index

			CD3DX12_DESCRIPTOR_RANGE subdCounter;
			subdCounter.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);
			m_RootSignature.AddDescriptorParameter(1, &subdCounter); // subd counter

			m_RootSignature.AddConstantBufferView(0); // object data
			m_RootSignature.AddConstantBufferView(1); // tessellation data
			m_RootSignature.AddConstantBufferView(2); // per frame data

			CD3DX12_DESCRIPTOR_RANGE drawArgs;
			drawArgs.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 6);
			m_RootSignature.AddDescriptorParameter(1, &drawArgs); // draw args

			m_RootSignature.Build(device, queueId);
			m_RootSignature.SetName(L"TessellationComputeRootSignature");

			m_UpdatePSO.SetRootSignature(&m_RootSignature);
			m_UpdatePSO.SetShader(&m_UpdateCS);
			m_UpdatePSO.Build(device, queueId);
			m_UpdatePSO.SetName(L"TessellationUpdatePSO");

			m_CopyDrawPSO.SetRootSignature(&m_RootSignature);
			m_CopyDrawPSO.SetShader(&m_CopyDrawCS);
			m_CopyDrawPSO.Build(device, queueId);
			m_CopyDrawPSO.SetName(L"TessellationCopyDrawPSO");

			D3D12_INDIRECT_ARGUMENT_DESC args[3];
			args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
			args[0].VertexBuffer.Slot = 0;
			args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
			args[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

			m_CommandSignature.SetByteStride(sizeof(IndirectCommand));
			m_CommandSignature.SetArguments(_countof(args), args);
			m_CommandSignature.Build(device);
			m_CommandSignature.SetName(L"TessellationCommandSignature");
		}

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.GetD3D12RootSignature(); }
		ID3D12CommandSignature* GetD3D12CommandSignature() const { return m_CommandSignature.GetD3D12Signature(); }
		ID3D12PipelineState* GetUpdatePSO() const { return m_UpdatePSO.GetD3D12PipelineState(); }
		ID3D12PipelineState* GetCopyDrawPSO() const { return m_CopyDrawPSO.GetD3D12PipelineState(); }
	};

	class TessellationDrawRootSignature
	{
	private:
		RootSignatureD3D12 m_RootSignature;

	public:
		TessellationDrawRootSignature(RenderDeviceD3D12* device)
		{
			CD3DX12_DESCRIPTOR_RANGE meshDataVertex;
			meshDataVertex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			m_RootSignature.AddDescriptorParameter(1, &meshDataVertex); // mesh data vertex

			CD3DX12_DESCRIPTOR_RANGE meshDataIndex;
			meshDataIndex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			m_RootSignature.AddDescriptorParameter(1, &meshDataIndex); // mesh data index

			CD3DX12_DESCRIPTOR_RANGE subdBufferOut;
			subdBufferOut.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
			m_RootSignature.AddDescriptorParameter(1, &subdBufferOut); // subd buffer

			CD3DX12_DESCRIPTOR_RANGE diffuseMap;
			diffuseMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
			m_RootSignature.AddDescriptorParameter(1, &diffuseMap); // diffuse map

			m_RootSignature.AddConstantBufferView(0); // object data
			m_RootSignature.AddConstantBufferView(1); // tessellation data
			m_RootSignature.AddConstantBufferView(2); // per frame data
			m_RootSignature.AddConstantBufferView(3); // shadow map frame data

			m_RootSignature.Build(device, QueueID::Direct);
			m_RootSignature.SetName(L"TessellationDrawRootSignature");
		}

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.GetD3D12RootSignature(); }
	};

	class TessellationShadowMapPass
	{
	public:
		struct PassConstants
		{
			XMFLOAT4X4 ViewProj;
			XMFLOAT3 CamPosition;
			float  DeltaTime;
			float  TotalTime;
			XMFLOAT3 Padding;
		};

	private:
		ShaderD3D12 m_VertexShader;
		PipelineStateD3D12 m_PSO;

	public:
		TessellationShadowMapPass(RenderDeviceD3D12* device, TessellationDrawRootSignature* rootSignature, D3D_SHADER_MACRO* macros = nullptr) :
			m_VertexShader(L"Shaders\\DefaultVS.hlsl", EDU_SHADER_TYPE_VERTEX, macros, "main", "vs_5_1")
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			auto rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			rast.CullMode = D3D12_CULL_MODE_NONE; // TODO: use D3D12_CULL_MODE_FRONT (tessellation algorithm will need to be modified)
			rast.DepthBias = 30000;
			rast.DepthBiasClamp = 0.0f;
			rast.SlopeScaledDepthBias = 1.0f;

			m_PSO.SetInputLayout({ inputLayout.data(), (UINT)inputLayout.size() });
			m_PSO.SetRootSignature(rootSignature->GetD3D12RootSignature());
			m_PSO.SetRasterizerState(rast);
			m_PSO.SetShader(&m_VertexShader);
			m_PSO.Build(device);
			m_PSO.SetName(L"ShadowMapPSO");
		}

		ID3D12PipelineState* GetD3D12PipelineState() const { return m_PSO.GetD3D12PipelineState(); }
	};

	class TessellationGBufferPass
	{
	public:
		static constexpr int GBufferCount = 2;
		static constexpr DXGI_FORMAT RtvFormats[GBufferCount] =
		{
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			DXGI_FORMAT_R8G8B8A8_SNORM
		};

	private:
		ShaderD3D12 m_VertexShader;
		ShaderD3D12 m_GeometryShader;
		ShaderD3D12 m_PixelShader;
		PipelineStateD3D12 m_PSO;

	public:
		TessellationGBufferPass(RenderDeviceD3D12* device, TessellationDrawRootSignature* rootSignature, bool wireframe, D3D_SHADER_MACRO* macros = nullptr) :
			m_VertexShader(L"Shaders\\DefaultVS.hlsl", EDU_SHADER_TYPE_VERTEX, macros, "main", "vs_5_1"),
			m_GeometryShader(L"Shaders\\WireframeGS.hlsl", EDU_SHADER_TYPE_GEOMETRY, macros, "main", "gs_5_1"),
			m_PixelShader(wireframe ? L"Shaders\\WireframePS.hlsl" : L"Shaders\\DefaultPS.hlsl", EDU_SHADER_TYPE_PIXEL, macros, "main", "ps_5_1")
		{
			std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			auto rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			rast.CullMode = D3D12_CULL_MODE_NONE; // TODO: use D3D12_CULL_MODE_FRONT (tessellation algorithm will need to be modified)

			m_PSO.SetInputLayout({ inputLayout.data(), (UINT)inputLayout.size() });
			m_PSO.SetRootSignature(rootSignature->GetD3D12RootSignature());
			m_PSO.SetShader(&m_VertexShader);
			if (wireframe) m_PSO.SetShader(&m_GeometryShader);
			m_PSO.SetShader(&m_PixelShader);
			m_PSO.SetRasterizerState(rast);
			m_PSO.SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			m_PSO.SetRTVFormats(GBufferCount, RtvFormats);
			m_PSO.SetDepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT);
			m_PSO.Build(device);
			m_PSO.SetName(wireframe ? L"TessellationWDrawPSO" : L"TessellationDrawPSO");
		}

		ID3D12PipelineState* GetD3D12PipelineState() const { return m_PSO.GetD3D12PipelineState(); }
	};

	class DeferredLightPass
	{
	public:
		struct PassConstants
		{
			DirectX::XMFLOAT4X4 ProjInv;
			DirectX::XMFLOAT4X4 ViewInv;
			DirectX::XMFLOAT4X4 View;
			DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
			UINT DirectionalLightsCount = 0;
			UINT PointLightsCount = 0;
			UINT SpotLightsCount = 0;
			UINT CascadeCount;
			UINT Padding;
			DirectX::XMFLOAT4 ClearColor;
			DirectX::XMFLOAT4 AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
			DirectX::XMFLOAT4X4 CascadeTransform[4];
			float CascadeDistance[4] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
		};

		struct MaterialConstants
		{
			XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
			float Roughness = 0.25f;
		};

		static constexpr DXGI_FORMAT AccumBuffFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	private:
		ShaderD3D12 m_VertexShader;
		ShaderD3D12 m_PixelShader;
		RootSignatureD3D12 m_RootSignature;
		PipelineStateD3D12 m_Pso;

	public:
		DeferredLightPass(RenderDeviceD3D12* device) :
			m_VertexShader(L"Shaders/DeferredLightPass.hlsl", EDU_SHADER_TYPE_VERTEX, nullptr, "VS", "vs_5_1"),
			m_PixelShader(L"Shaders/DeferredLightPass.hlsl", EDU_SHADER_TYPE_PIXEL, nullptr, "PS", "ps_5_1")
		{
			CD3DX12_DESCRIPTOR_RANGE albedoTex;
			albedoTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			m_RootSignature.AddDescriptorParameter(1, &albedoTex); // albedo texture

			CD3DX12_DESCRIPTOR_RANGE normalTex;
			normalTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			m_RootSignature.AddDescriptorParameter(1, &normalTex); // normal texture

			CD3DX12_DESCRIPTOR_RANGE depthTex;
			depthTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
			m_RootSignature.AddDescriptorParameter(1, &depthTex); // depth texture

			CD3DX12_DESCRIPTOR_RANGE lights;
			lights.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);
			m_RootSignature.AddDescriptorParameter(1, &lights); // lights

			CD3DX12_DESCRIPTOR_RANGE shadowMap;
			shadowMap.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 4);
			m_RootSignature.AddDescriptorParameter(1, &shadowMap); // shadow map

			m_RootSignature.AddConstantBufferView(0); // pass constants
			m_RootSignature.AddConstantBufferView(1); // material constants

			m_RootSignature.Build(device, QueueID::Direct);

			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			auto dss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			dss.DepthEnable = false;

			DXGI_FORMAT rtvFormats[1] = { AccumBuffFormat };

			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetRootSignature(&m_RootSignature);
			m_Pso.SetDepthStencilState(dss);
			m_Pso.SetRTVFormat(AccumBuffFormat);
			m_Pso.SetShader(&m_VertexShader);
			m_Pso.SetShader(&m_PixelShader);
			m_Pso.Build(device);
			m_Pso.SetName(L"DeferredLightPSO");
		}

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.GetD3D12RootSignature(); }
		ID3D12PipelineState* GetD3D12PipelineState() const { return m_Pso.GetD3D12PipelineState(); }
	};

	class MotionBlurPass
	{
	public:
		struct PassData
		{
			XMFLOAT4X4 ViewProjInv;
			XMFLOAT4X4 PreviousViewProj;
		};

	private:
		ShaderD3D12 m_VertexShader;
		ShaderD3D12 m_PixelShader;
		RootSignatureD3D12 m_RootSignature;
		PipelineStateD3D12 m_Pso;

	public:
		MotionBlurPass(RenderDeviceD3D12* device, D3D_SHADER_MACRO* macros = nullptr) :
			m_VertexShader(L"Shaders/MotionBlur.hlsl", EDU_SHADER_TYPE_VERTEX, macros, "VS", "vs_5_1"),
			m_PixelShader(L"Shaders/MotionBlur.hlsl", EDU_SHADER_TYPE_PIXEL, macros, "PS", "ps_5_1")
		{
			CD3DX12_DESCRIPTOR_RANGE accumTex;
			accumTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			m_RootSignature.AddDescriptorParameter(1, &accumTex); // accumulation buffer

			CD3DX12_DESCRIPTOR_RANGE depthTex;
			depthTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			m_RootSignature.AddDescriptorParameter(1, &depthTex); // depth buffer

			m_RootSignature.AddConstantBufferView(0); // pass data
			m_RootSignature.AddConstants(4, 1); // pass constants

			m_RootSignature.Build(device, QueueID::Direct);
			m_RootSignature.SetName(L"MotionBlurRootSignature");

			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			auto dss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			dss.DepthEnable = false;

			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetRootSignature(&m_RootSignature);
			m_Pso.SetDepthStencilState(dss);
			m_Pso.SetShader(&m_VertexShader);
			m_Pso.SetShader(&m_PixelShader);
			m_Pso.SetRTVFormat(DeferredLightPass::AccumBuffFormat);
			m_Pso.Build(device);
			m_Pso.SetName(L"MotionBlurPSO");
		}

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.GetD3D12RootSignature(); }
		ID3D12PipelineState* GetD3D12PipelineState() const { return m_Pso.GetD3D12PipelineState(); }
	};

	class BloomPass
	{
	private:
		ShaderD3D12 m_VertexShader;
		ShaderD3D12 m_PSThreshold;
		ShaderD3D12 m_PSBlurH;
		ShaderD3D12 m_PSBlurV;
		ShaderD3D12 m_PSUpscale;
		RootSignatureD3D12 m_RootSignature;
		PipelineStateD3D12 m_TresholdPso;
		PipelineStateD3D12 m_HPso;
		PipelineStateD3D12 m_VPso;
		PipelineStateD3D12 m_UpscalePso;

	public:
		BloomPass(RenderDeviceD3D12* device, D3D_SHADER_MACRO* macros = nullptr) :
			m_VertexShader(L"Shaders/Bloom.hlsl", EDU_SHADER_TYPE_VERTEX, macros, "VS", "vs_5_1"),
			m_PSThreshold(L"Shaders/Bloom.hlsl", EDU_SHADER_TYPE_PIXEL, macros, "PSThreshold", "ps_5_1"),
			m_PSBlurH(L"Shaders/Bloom.hlsl", EDU_SHADER_TYPE_PIXEL, macros, "PSBlurH", "ps_5_1"),
			m_PSBlurV(L"Shaders/Bloom.hlsl", EDU_SHADER_TYPE_PIXEL, macros, "PSBlurV", "ps_5_1"),
			m_PSUpscale(L"Shaders/Bloom.hlsl", EDU_SHADER_TYPE_PIXEL, macros, "PSUpscale", "ps_5_1")
		{
			CD3DX12_DESCRIPTOR_RANGE bloomTex0;
			bloomTex0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			m_RootSignature.AddDescriptorParameter(1, &bloomTex0); // bloom buffer 0

			CD3DX12_DESCRIPTOR_RANGE bloomTex1;
			bloomTex1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			m_RootSignature.AddDescriptorParameter(1, &bloomTex1); // bloom buffer 1

			m_RootSignature.AddConstants(8, 0);

			m_RootSignature.Build(device, QueueID::Direct);
			m_RootSignature.SetName(L"BloomRootSignature");

			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			auto dss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			dss.DepthEnable = false;

			auto rast = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			rast.DepthClipEnable = false;

			m_TresholdPso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_TresholdPso.SetRootSignature(&m_RootSignature);
			m_TresholdPso.SetDepthStencilState(dss);
			m_TresholdPso.SetRasterizerState(rast);
			m_TresholdPso.SetShader(&m_VertexShader);
			m_TresholdPso.SetShader(&m_PSThreshold);
			m_TresholdPso.SetRTVFormat(DeferredLightPass::AccumBuffFormat);
			m_TresholdPso.Build(device);
			m_TresholdPso.SetName(L"ThresholdBloomPSO");

			m_HPso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_HPso.SetRootSignature(&m_RootSignature);
			m_HPso.SetDepthStencilState(dss);
			m_HPso.SetRasterizerState(rast);
			m_HPso.SetShader(&m_VertexShader);
			m_HPso.SetShader(&m_PSBlurH);
			m_HPso.SetRTVFormat(DeferredLightPass::AccumBuffFormat);
			m_HPso.Build(device);
			m_HPso.SetName(L"BloomHPSO");

			m_VPso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_VPso.SetRootSignature(&m_RootSignature);
			m_VPso.SetDepthStencilState(dss);
			m_VPso.SetRasterizerState(rast);
			m_VPso.SetShader(&m_VertexShader);
			m_VPso.SetShader(&m_PSBlurV);
			m_VPso.SetRTVFormat(DeferredLightPass::AccumBuffFormat);
			m_VPso.Build(device);
			m_VPso.SetName(L"BloomVPSO");

			m_UpscalePso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_UpscalePso.SetRootSignature(&m_RootSignature);
			m_UpscalePso.SetDepthStencilState(dss);
			m_UpscalePso.SetRasterizerState(rast);
			m_UpscalePso.SetShader(&m_VertexShader);
			m_UpscalePso.SetShader(&m_PSUpscale);
			m_UpscalePso.SetRTVFormat(DeferredLightPass::AccumBuffFormat);
			m_UpscalePso.Build(device);
			m_UpscalePso.SetName(L"BloomUpscalePSO");
		}

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.GetD3D12RootSignature(); }
		ID3D12PipelineState* GetD3D12PipelineStateThreshold() const { return m_TresholdPso.GetD3D12PipelineState(); }
		ID3D12PipelineState* GetD3D12PipelineStateH() const { return m_HPso.GetD3D12PipelineState(); }
		ID3D12PipelineState* GetD3D12PipelineStateV() const { return m_VPso.GetD3D12PipelineState(); }
		ID3D12PipelineState* GetD3D12PipelineStateUpscale() const { return m_UpscalePso.GetD3D12PipelineState(); }
	};

	class ToneMappingPass
	{
	private:
		ShaderD3D12 m_VertexShader;
		ShaderD3D12 m_PixelShader;
		RootSignatureD3D12 m_RootSignature;
		PipelineStateD3D12 m_Pso;

	public:
		ToneMappingPass(RenderDeviceD3D12* device) :
			m_VertexShader(L"Shaders/ToneMapping.hlsl", EDU_SHADER_TYPE_VERTEX, nullptr, "VS", "vs_5_1"),
			m_PixelShader(L"Shaders/ToneMapping.hlsl", EDU_SHADER_TYPE_PIXEL, nullptr, "PS", "ps_5_1")
		{
			CD3DX12_DESCRIPTOR_RANGE accumTex;
			accumTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			m_RootSignature.AddDescriptorParameter(1, &accumTex); // accumulation buffer

			CD3DX12_DESCRIPTOR_RANGE bloomTex;
			bloomTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
			m_RootSignature.AddDescriptorParameter(1, &bloomTex); // bloom buffer

			m_RootSignature.Build(device, QueueID::Direct);

			std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};

			auto dss = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			dss.DepthEnable = false;

			m_Pso.SetInputLayout({ mInputLayout.data(), (UINT)mInputLayout.size() });
			m_Pso.SetRootSignature(&m_RootSignature);
			m_Pso.SetDepthStencilState(dss);
			m_Pso.SetShader(&m_VertexShader);
			m_Pso.SetShader(&m_PixelShader);
			m_Pso.SetRTVFormat(DXGI_FORMAT_R8G8B8A8_UNORM);
			m_Pso.Build(device);
			m_Pso.SetName(L"ToneMappingPSO");
		}

		ID3D12RootSignature* GetD3D12RootSignature() const { return m_RootSignature.GetD3D12RootSignature(); }
		ID3D12PipelineState* GetD3D12PipelineState() const { return m_Pso.GetD3D12PipelineState(); }
	};
}