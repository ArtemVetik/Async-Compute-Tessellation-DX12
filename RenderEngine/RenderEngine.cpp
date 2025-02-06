#include "RenderEngine.h"
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
		m_RenderType(RenderType::Direct)
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

		Microsoft::WRL::ComPtr<ID3D12Device> device;
		HRESULT hardwareResult = D3D12CreateDevice(
			nullptr,
			D3D_FEATURE_LEVEL_11_0,
			IID_PPV_ARGS(device.GetAddressOf()));

		if (FAILED(hardwareResult))
			throw;

		m_Device = std::make_unique<RenderDeviceD3D12>(device);

		m_SwapChain = std::make_unique<SwapChain>(m_Device.get(),
			mainWindow.GetClientWidth(), mainWindow.GetClientHeight(), mainWindow.GetMainWindow());

		m_Camera = std::make_unique<Camera>(m_Device.get(), mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		Resize(mainWindow.GetClientWidth(), mainWindow.GetClientHeight());

		m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT).Reset();

		m_ImGuiTex = m_Device->AllocateGPUDescriptor(QueueID::Direct, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
		InitImGui(mainWindow);

		m_PsoData = std::make_unique<TessellationPSOData>(m_Device.get());
		m_SSQuad = std::make_unique<ScreenSpaceQuad>(m_Device.get());
		m_AdaptiveTessellation = std::make_unique<AdaptiveTessellationCompute>(m_Device.get(), m_SwapChain.get(), m_Camera.get(), m_PsoData.get(), m_RenderType != RenderType::Direct);
		m_AdaptiveTessellationDraw = std::make_unique<AdaptiveTessellationDraw>(m_Device.get(), m_SwapChain.get(), m_PsoData.get());
		m_CSMRendering = std::make_unique<CSMRendering>(m_Device.get(), m_PsoData.get());
		m_DeferredLightRendering = std::make_unique<DeferredLightRendering>(m_Device.get(), m_SwapChain.get(), m_SSQuad.get());
		m_MotionBlurRendering = std::make_unique<MotionBlurRendering>(m_Device.get(), m_SwapChain.get(), m_SSQuad.get());
		m_BloomRendering = std::make_unique<BloomRendering>(m_Device.get(), m_SSQuad.get());

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

		m_AdaptiveTessellation->Compute(m_Timer);

		if (Light* shadowLight = m_DeferredLightRendering->GetShadowLight())
			m_CSMRendering->Render(m_Camera.get(), shadowLight, m_Timer, m_AdaptiveTessellation.get());

		dCommandContext.SetViewports(&m_Viewport, 1);
		dCommandContext.SetScissorRects(&m_ScissorRect, 1);
		m_AdaptiveTessellationDraw->Draw(m_DeferredLightRendering->GetGBuffer());
		m_AdaptiveTessellation->ExecuteIndirect();

		m_DeferredLightRendering->RenderLights(m_Camera.get(), m_CSMRendering.get());
		m_BloomRendering->Render(m_DeferredLightRendering->GetGBuffer());
		m_MotionBlurRendering->Render(m_Camera.get(), m_DeferredLightRendering->GetGBuffer());
		m_DeferredLightRendering->RenderToneMapping(m_Camera.get(), m_BloomRendering.get());

		dCommandContext.SetRenderTargets(1, &(m_SwapChain->CurrentBackBufferView()), true, &(m_SwapChain->DepthStencilView()));
		RecordImGuiCommands();

		dCommandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_SwapChain->CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
		dCommandContext.FlushResourceBarriers();

		if (frameRenderType == RenderType::AsyncAll)
			dCommandQueue.Wait(&cCommandQueue, cCommandQueue.GetNextCmdListNum());

		dCommandQueue.CloseAndExecuteCommandContext(&dCommandContext);
		dCommandContext.Reset();

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
			Window::GetInstance()->SetPosition(m_PendingResize.x, m_PendingResize.y, m_PendingResize.width, m_PendingResize.height);
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

		if (InputManager::GetInstance().IsKeyPressed(DIK_1))
			m_Camera->RotateY(timer.GetDeltaTime() * 0.2f);
		if (InputManager::GetInstance().IsKeyPressed(DIK_2))
			m_Camera->RotateY(timer.GetDeltaTime() * 0.4f);
		if (InputManager::GetInstance().IsKeyPressed(DIK_3))
			m_Camera->RotateY(timer.GetDeltaTime() * 0.6f);

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

		m_Camera->Update();
	}

	void RenderEngine::RecordImGuiCommands()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("App parameters | TEST");
		ImGui::Text("Test application parameters.");

		if (ImGui::Combo("Render Type", (int*)&m_RenderType, "Direct\0Async All\0\0"))
			m_AdaptiveTessellation->ForceRebuildAll(m_RenderType != RenderType::Direct);

		m_AdaptiveTessellation->RenderImGui();
		m_DeferredLightRendering->RenderImGui(m_Timer);
		m_MotionBlurRendering->RenderImGui();
		m_BloomRendering->RenderImGui();

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
		(void)io;
		ImGui::StyleColorsDark();
		ImGui_ImplWin32_Init(mainWindow.GetMainWindow());

		ImGui_ImplDX12_Init(
			m_Device->GetD3D12Device(),
			3,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			m_Device->GetD3D12DescriptorHeap(),
			m_ImGuiTex.GetCpuHandle(),
			m_ImGuiTex.GetGpuHandle()
		);
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