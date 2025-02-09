#pragma once

#include <memory>

#include "framework.h"
#include "Window.h"
#include "Timer.h"
#include "Camera.h"
#include "AdaptiveTessellationCompute.h"
#include "AdaptiveTessellationDraw.h"
#include "CSMRendering.h"
#include "DeferredLightRendering.h"
#include "MotionBlurRendering.h"
#include "BloomRendering.h"
#include "GPUStatsUI.h"

#include "../Core/Graphics/SwapChain.h"
#include "../Core/EduMath/SimpleMath.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

	enum class RenderType
	{
		Direct,
		AsyncAll,
		AsyncShadowMap,
		AsyncDraw,
		AsyncPostProcess,
	};

	class RENDERENGINE_API RenderEngine
	{
	public:
		RenderEngine(const Timer& timer);
		~RenderEngine();

		RenderEngine(const RenderEngine& rhs) = delete;
		RenderEngine& operator=(const RenderEngine& rhs) = delete;

		bool StartUp(const Window& mainWindow);

		void Render();
		void Update(const Timer& timer);

		void RecordImGuiCommands();
		void PendingResize(UINT w, UINT h);

		static RenderEngine* GetInstance();

		void AllocImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle);
		void FreeImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);
	
	private:
		void InitImGui(const Window& mainWindow);
		void Resize(UINT w, UINT h);

	private:
		static RenderEngine* m_Instance;

		std::unique_ptr<RenderDeviceD3D12> m_Device;
		std::unique_ptr<SwapChain> m_SwapChain;

		std::unique_ptr<Camera> m_Camera;
		std::unique_ptr<TessellationPSOData> m_PsoData;
		std::unique_ptr<ScreenSpaceQuad> m_SSQuad;
		std::unique_ptr<AdaptiveTessellationCompute> m_AdaptiveTessellation;
		std::unique_ptr<AdaptiveTessellationDraw> m_AdaptiveTessellationDraw;
		std::unique_ptr<CSMRendering> m_CSMRendering;
		std::unique_ptr<DeferredLightRendering> m_DeferredLightRendering;
		std::unique_ptr<MotionBlurRendering> m_MotionBlurRendering;
		std::unique_ptr<BloomRendering> m_BloomRendering;
		std::unique_ptr<GPUStatsUI> m_RenderStats;

		DescriptorHeapAllocation m_ImGuiTex;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		const Timer& m_Timer;

		static constexpr DirectX::SimpleMath::Rectangle EmptyResize = { -1, -1, -1, -1 };
		DirectX::SimpleMath::Rectangle m_PendingResize = EmptyResize;

		RenderType m_RenderType;
		bool m_WaitForCompute;
	};
}