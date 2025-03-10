#include <dxgi1_6.h>
#include <iostream>

#include "RenderEngine.h"
#include "Common.h"
#include "shellapi.h"

#include "WinPixEventRuntime/pix3.h"
#include "../Core/InputSystem/InputManager.h"

namespace AsyncComputeTessellation
{
	RenderEngine* RenderEngine::m_Instance = nullptr;

	RenderEngine* RenderEngine::GetInstance()
	{
		return m_Instance;
	}

	RenderEngine::RenderEngine(const Timer& timer) :
		m_Timer(timer),
		m_Viewport{},
		m_ScissorRect{},
		m_RenderType(RenderType::Direct),
		m_WaitForCompute(false)
	{
		assert(m_Instance == nullptr);
		m_Instance = this;
	}

	RenderEngine::~RenderEngine()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		if (m_Device != nullptr)
			m_Device->FlushQueues();
	}

	bool RenderEngine::StartUp(const Window& mainWindow)
	{
#if defined(DEBUG) || defined(_DEBUG) 
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
		debugController->EnableDebugLayer();
#endif

		IDXGIFactory6* pFactory = nullptr;
		if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&pFactory)))
		{
			std::cerr << "Failed to create DXGI Factory!" << std::endl;
			return false;
		}

		IDXGIAdapter1* pAdapter = nullptr;

		if (FAILED(pFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter1), (void**)&pAdapter)))
		{
			std::cerr << "Failed to get high-performance GPU!" << std::endl;
			pFactory->Release();
			return false;
		}

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		HRESULT hardwareResult = D3D12CreateDevice(
			pAdapter,
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(device.GetAddressOf()));

		if (FAILED(hardwareResult))
		{
			std::cerr << "The selected GPU does not support DirectX 12!" << std::endl;
			pAdapter->Release();
			pFactory->Release();
			return false;
		}

		pAdapter->GetDesc1(&m_DeviceDesc);

		pAdapter->Release();
		pFactory->Release();

		m_Device = std::make_unique<RenderDeviceD3D12>(device);

		m_SwapChain = std::make_unique<SwapChain>(m_Device.get(),
			mainWindow.GetClientWidth(), mainWindow.GetClientHeight(), mainWindow.GetMainWindow());

		m_Camera = std::make_unique<Camera>(m_Device.get(), mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		Resize(mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT).Reset();

		InitImGui(mainWindow);

		m_PsoData = std::make_unique<TessellationPSOData>(m_Device.get());
		m_SSQuad = std::make_unique<ScreenSpaceQuad>(m_Device.get());
		m_AdaptiveTessellation = std::make_unique<AdaptiveTessellationCompute>(m_Device.get(), m_SwapChain.get(), m_Camera.get(), m_PsoData.get(), m_RenderType != RenderType::Direct);
		m_AdaptiveTessellationDraw = std::make_unique<AdaptiveTessellationDraw>(m_Device.get(), m_SwapChain.get(), m_PsoData.get());
		m_CSMRendering = std::make_unique<CSMRendering>(m_Device.get(), m_PsoData.get());
		m_DeferredLightRendering = std::make_unique<DeferredLightRendering>(m_Device.get(), m_SwapChain.get(), m_SSQuad.get());
		m_MotionBlurRendering = std::make_unique<MotionBlurRendering>(m_Device.get(), m_SwapChain.get(), m_SSQuad.get());
		m_BloomRendering = std::make_unique<BloomRendering>(m_Device.get(), m_SSQuad.get());
		m_RenderStats = std::make_unique<GPUStatsUI>(m_Device.get(), m_Camera.get());
		m_RenderSettings = std::make_unique<RenderSettings>(m_AdaptiveTessellation.get(), m_DeferredLightRendering.get(), m_MotionBlurRendering.get(), m_BloomRendering.get(), m_Camera.get());

		m_BloomRendering->Resize(m_SwapChain->GetWidth(), m_SwapChain->GetHeight());

		return true;
	}

	void RenderEngine::Render()
	{
		RenderType frameRenderType = m_RenderType;

		ID3D12DescriptorHeap* descriptorHeaps[] = { m_Device->GetD3D12DescriptorHeap() };

		auto& dCommandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto& cCommandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_COMPUTE);
		auto& dCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto& cCommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);

		dCommandContext.Reset();
		dCommandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
		dCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		if (frameRenderType != RenderType::Direct)
		{
			cCommandContext.Reset();
			cCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		}

		m_RenderStats->MarkRenderStart(GPUStatsUI::RenderStatType::Total);
		m_RenderStats->MarkComputeStart(m_RenderType != RenderType::Direct);

		m_AdaptiveTessellation->Compute(m_Timer);
		m_RenderStats->MarkComputeEnd(m_RenderType != RenderType::Direct);

		m_RenderStats->MarkRenderStart(GPUStatsUI::RenderStatType::ShadowMap);
		m_AdaptiveTessellation->PrepareDraw();

		if (Light* shadowLight = m_DeferredLightRendering->GetShadowLight())
			m_CSMRendering->Render(m_Camera.get(), shadowLight, m_Timer, m_AdaptiveTessellation.get());
		m_RenderStats->MarkRenderEnd(GPUStatsUI::RenderStatType::ShadowMap);

		if (frameRenderType == RenderType::AsyncShadowMap)
		{
			dCommandQueue.Signal();
			dCommandQueue.CloseAndExecuteCommandContext(&dCommandContext);
			dCommandContext.Reset();

			cCommandQueue.Wait(&dCommandQueue, dCommandQueue.GetNextCmdListNum() - 1);
			cCommandQueue.CloseAndExecuteCommandContext(&cCommandContext);
			cCommandContext.Reset();

			if (m_WaitForCompute)
				dCommandQueue.Wait(&cCommandQueue, cCommandQueue.GetNextCmdListNum());

			dCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			cCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			m_AdaptiveTessellation->PrepareDraw();
		}

		m_RenderStats->MarkRenderStart(GPUStatsUI::RenderStatType::Draw);
		PIXBeginEvent(dCommandContext.GetCmdList(), PIX_COLOR(255, 255, 255), L"GBuffer Draw");
		dCommandContext.SetViewports(&m_Viewport, 1);
		dCommandContext.SetScissorRects(&m_ScissorRect, 1);
		m_AdaptiveTessellationDraw->Draw(m_DeferredLightRendering->GetGBuffer());
		m_AdaptiveTessellation->ExecuteIndirect();
		m_RenderStats->MarkRenderEnd(GPUStatsUI::RenderStatType::Draw);
		PIXEndEvent(dCommandContext.GetCmdList());

		if (frameRenderType == RenderType::AsyncDraw)
		{
			dCommandQueue.Signal();
			dCommandQueue.CloseAndExecuteCommandContext(&dCommandContext);
			dCommandContext.Reset();

			cCommandQueue.Wait(&dCommandQueue, dCommandQueue.GetNextCmdListNum() - 1);
			cCommandQueue.CloseAndExecuteCommandContext(&cCommandContext);
			cCommandContext.Reset();

			if (m_WaitForCompute)
				dCommandQueue.Wait(&cCommandQueue, cCommandQueue.GetNextCmdListNum());

			dCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			cCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

			dCommandContext.SetViewports(&m_Viewport, 1);
			dCommandContext.SetScissorRects(&m_ScissorRect, 1);
			m_AdaptiveTessellation->PrepareDraw();
		}

		m_RenderStats->MarkRenderStart(GPUStatsUI::RenderStatType::Light);
		m_DeferredLightRendering->RenderLights(m_Camera.get(), m_CSMRendering.get());
		m_RenderStats->MarkRenderEnd(GPUStatsUI::RenderStatType::Light);

		if (frameRenderType == RenderType::AsyncPostProcess)
		{
			dCommandQueue.CloseAndExecuteCommandContext(&dCommandContext);
			dCommandContext.Reset();

			cCommandQueue.Wait(&dCommandQueue, dCommandQueue.GetNextCmdListNum());
			cCommandQueue.CloseAndExecuteCommandContext(&cCommandContext);
			cCommandContext.Reset();

			dCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			cCommandContext.GetCmdList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			dCommandContext.SetViewports(&m_Viewport, 1);
			dCommandContext.SetScissorRects(&m_ScissorRect, 1);
		}

		m_RenderStats->MarkRenderStart(GPUStatsUI::RenderStatType::PostProcess);
		PIXBeginEvent(dCommandContext.GetCmdList(), PIX_COLOR(128, 0, 128), L"Post Process");
		m_BloomRendering->Render(m_DeferredLightRendering->GetGBuffer());

		dCommandContext.SetViewports(&m_Viewport, 1);
		dCommandContext.SetScissorRects(&m_ScissorRect, 1);
		m_MotionBlurRendering->Render(m_Camera.get(), m_DeferredLightRendering->GetGBuffer(), &m_Timer);
		m_DeferredLightRendering->RenderToneMapping(m_Camera.get(), m_BloomRendering.get());

		dCommandContext.SetRenderTargets(1, &(m_SwapChain->CurrentBackBufferView()), true, &(m_SwapChain->DepthStencilView()));
		RecordImGuiCommands();

		dCommandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		dCommandContext.FlushResourceBarriers();

		if (frameRenderType == RenderType::AsyncAll)
			dCommandQueue.Wait(&cCommandQueue, cCommandQueue.GetNextCmdListNum());

		m_RenderStats->MarkRenderEnd(GPUStatsUI::RenderStatType::PostProcess);
		PIXEndEvent(dCommandContext.GetCmdList());
		m_RenderStats->MarkRenderEnd(GPUStatsUI::RenderStatType::Total);
		dCommandQueue.CloseAndExecuteCommandContext(&dCommandContext);
		dCommandContext.Reset();

		if (frameRenderType == RenderType::AsyncPostProcess && m_WaitForCompute)
			dCommandQueue.Wait(&cCommandQueue, cCommandQueue.GetNextCmdListNum());

		if (frameRenderType == RenderType::AsyncAll)
		{
			cCommandQueue.CloseAndExecuteCommandContext(&cCommandContext);
			cCommandContext.Reset();
			cCommandQueue.Wait(&dCommandQueue, dCommandQueue.GetNextCmdListNum());
		}

		m_SwapChain->Present();
		m_Device->FinishFrame();

		// Resize should be at the end of the frame after the main ExecuteCommandList and FinishFrame.
		// Since FinishFrame also occurs inside swapChain->Resize(), and it is not desirable
		// that resources are removed before the end of rendering
		if (m_PendingResize != EmptyResize)
		{
			Resize(m_PendingResize.width, m_PendingResize.height);
			m_PendingResize = EmptyResize;
			dCommandContext.Reset();
		}
	}

	void RenderEngine::Update(const Timer& timer)
	{
		XMVECTOR direction = XMLoadFloat3(&m_Camera->GetLook());
		XMVECTOR lrVector = XMLoadFloat3(&m_Camera->GetRight());
		XMVECTOR upVector = XMLoadFloat3(&m_Camera->GetUp());

		float moveScale = 35.0f;
		static constexpr float rotateScale = 0.01f;
		static constexpr float rotateLerpSpeed = 20.0f;

		if (InputManager::GetInstance().IsKeyPressed(DIK_LSHIFT))
			moveScale *= 2;

		if (InputManager::GetInstance().IsKeyPressed(DIK_W))
			m_Camera->Move(direction * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_S))
			m_Camera->Move(-direction * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_A))
			m_Camera->Move(-lrVector * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_D))
			m_Camera->Move(lrVector * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_E))
			m_Camera->Move(upVector * moveScale * timer.GetDeltaTime());
		if (InputManager::GetInstance().IsKeyPressed(DIK_Q))
			m_Camera->Move(-upVector * moveScale * timer.GetDeltaTime());

		auto mouseState = InputManager::GetInstance().GetMouseState();

		static XMFLOAT2 currentDelta = { 0, 0 };
		static XMFLOAT2 targetDelta = { 0, 0 };

		if ((mouseState.rgbButtons[1] & 0x80) != 0)
		{
			targetDelta.x += mouseState.lX * rotateScale;
			targetDelta.y += mouseState.lY * rotateScale;
		}

		auto Lerp = [](float a, float b, float t) {
			return a + (b - a) * t;
			};

		float prevX = currentDelta.x;
		currentDelta.x = Lerp(currentDelta.x, targetDelta.x, timer.GetDeltaTime() * rotateLerpSpeed);
		m_Camera->RotateY(currentDelta.x - prevX);

		float prevY = currentDelta.y;
		currentDelta.y = Lerp(currentDelta.y, targetDelta.y, timer.GetDeltaTime() * rotateLerpSpeed);
		m_Camera->Pitch(currentDelta.y - prevY);

		m_Camera->Update(timer);
		m_RenderStats->Update(m_RenderType != RenderType::Direct);
	}

	void RenderEngine::RecordImGuiCommands()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("App Parameters");
		ImGui::Text("Async Compute Tessellation");

		if (ImGui::Combo("Render Type", (int*)&m_RenderType, "Direct\0Async All\0Async Shadow Map\0Async Draw\0Async Post Process\0\0"))
			m_AdaptiveTessellation->ForceRebuildAll(m_RenderType != RenderType::Direct);

		ImGui::Checkbox("Wait For Compute", &m_WaitForCompute);

		static bool cameraRotate;

		if (ImGui::Checkbox("Rotate Around", &cameraRotate))
			m_Camera->SetRotateAroundMode(cameraRotate);

		ImGui::SameLine();

		ImGui::Text("%f", atan2f(m_Camera->GetPosition().x, m_Camera->GetPosition().z));

		ImGui::SeparatorText("Settings");

		m_AdaptiveTessellation->RenderImGui();
		m_DeferredLightRendering->RenderImGui(m_Timer);
		m_MotionBlurRendering->RenderImGui();
		m_BloomRendering->RenderImGui();
		m_RenderStats->RenderImGui();

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 windowSize = ImVec2(300, 100);
		ImVec2 windowPos = ImVec2(io.DisplaySize.x - windowSize.x - 10, io.DisplaySize.y - windowSize.y - 10);

		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

		ImGui::Begin("GPU Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

		ImGui::TextWrapped("Primary GPU: %ls", m_DeviceDesc.Description);
		ImGui::Text("\tVideo Memory: %llu", m_DeviceDesc.DedicatedVideoMemory / (1024 * 1024));
		ImGui::Text("\tSystem Memory: %llu MB", m_DeviceDesc.DedicatedSystemMemory / (1024 * 1024));
		ImGui::Text("\tShared Memory: %llu MB", m_DeviceDesc.SharedSystemMemory / (1024 * 1024));
		ImGui::Text("\tVendor ID: %u, Device ID: %u", m_DeviceDesc.VendorId, m_DeviceDesc.DeviceId);

		ImGui::End();

		ImGui::SeparatorText("Import/Export");

		if (ImGui::Button("Export Settings"))
		{
			auto configFile = Common::OpenFolderDialog(false, L"Select the file to write the config to");
			m_RenderSettings->Export(configFile);
		}

		ImGui::SameLine();

		if (ImGui::Button("Import Settings"))
		{
			auto configFile = Common::OpenFolderDialog(false, L"Select the config file");
			m_RenderSettings->Import(configFile);
		}

		ImGui::SeparatorText("Author");

		if (ImGui::Selectable("https://github.com/ArtemVetik", false, ImGuiSelectableFlags_None))
		{
			ShellExecute(0, L"open", L"https://github.com/ArtemVetik", 0, 0, SW_SHOWNORMAL);
		}

		ImGui::End();

		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT).GetCmdList());
	}

	void RenderEngine::PendingResize(UINT w, UINT h)
	{
		UINT lx, ly, lw, lh;
		Window::GetInstance()->GetPosition(lx, ly, lw, lh);

		if (lw != w || lh != h)
			m_PendingResize = { (long)lx, (long)ly, (long)w, (long)h };
	}

	void RenderEngine::InitImGui(const Window& mainWindow)
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(mainWindow.GetMainWindow());

		ImGui_ImplDX12_InitInfo init_info;
		init_info.Device = m_Device->GetD3D12Device();
		init_info.CommandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetD3D12CommandQueue();
		init_info.NumFramesInFlight = 3;
		init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		init_info.SrvDescriptorHeap = m_Device->GetD3D12DescriptorHeap();

		init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
			{
				return RenderEngine::GetInstance()->AllocImGuiSrv(info, out_cpu_handle, out_gpu_handle);
			};

		init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
			{
				return RenderEngine::GetInstance()->FreeImGuiSrv(info, cpu_handle, gpu_handle);
			};

		ImGui_ImplDX12_Init(&init_info);
	}

	void RenderEngine::AllocImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
	{
		m_ImGuiTex = m_Device->AllocateGPUDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		*out_cpu_handle = m_ImGuiTex.GetCpuHandle();
		*out_gpu_handle = m_ImGuiTex.GetGpuHandle();
	}

	void RenderEngine::FreeImGuiSrv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
	{
		m_ImGuiTex = {};
	}

	void RenderEngine::Resize(UINT w, UINT h)
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto& commandQueue = m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.Reset();
		m_SwapChain->Resize(w, h);

		if (m_DeferredLightRendering)
			m_DeferredLightRendering->GetGBuffer()->Resize(m_Device.get(), w, h);
		if (m_BloomRendering)
			m_BloomRendering->Resize(w, h);

		commandContext.FlushResourceBarriers();
		commandQueue.CloseAndExecuteCommandContext(&commandContext);

		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;
		m_Viewport.Width = w;
		m_Viewport.Height = h;
		m_Viewport.MinDepth = 0.0f;
		m_Viewport.MaxDepth = 1.0f;

		m_ScissorRect = { 0, 0, (int)w, (int)h };

		m_Camera->SetProjectionMatrix(w, h);
	}
}