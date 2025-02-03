#pragma once

#include <memory>

#include "framework.h"
#include "Window.h"
#include "Timer.h"
#include "Camera.h"
#include "AdaptiveTessellation.h"
#include "DeferredLightRendering.h"

#include "../Core/Graphics/SwapChain.h"
#include "../Core/EduMath/SimpleMath.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

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

	private:
		void InitImGui(const Window& mainWindow);
		void Resize(UINT w, UINT h);

	private:
		static RenderEngine* m_Instance;

		std::unique_ptr<RenderDeviceD3D12> m_Device;
		std::unique_ptr<SwapChain> m_SwapChain;

		std::unique_ptr<Camera> m_Camera;
		std::unique_ptr<AdaptiveTessellation> m_AdaptiveTessellation;
		std::unique_ptr<DeferredLightRendering> m_DeferredLightRendering;

		DescriptorHeapAllocation m_ImGuiTex;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		const Timer& m_Timer;

		static constexpr DirectX::SimpleMath::Rectangle EmptyResize = { -1, -1, -1, -1 };
		DirectX::SimpleMath::Rectangle m_PendingResize = EmptyResize;
	};
}