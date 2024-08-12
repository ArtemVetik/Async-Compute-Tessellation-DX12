#include "DXCore.h"
#include "WindowsX.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DXCore::GetApplication()->MsgProc(hwnd, msg, wParam, lParam);
}

DXCore* DXCore::Instance = nullptr;
DXCore* DXCore::GetApplication()
{
	return Instance;
}

DXCore::DXCore(HINSTANCE hInstance) : applicationInstanceHandle(hInstance)
{
	assert(Instance == nullptr);
	Instance = this;
}

DXCore::~DXCore()
{
	if (Device != nullptr)
		FlushCommandQueue();
}

HINSTANCE DXCore::ApplicationInstance() const
{
	return applicationInstanceHandle;
}

HWND DXCore::MainWindow() const
{
	return mainWindowHandle;
}

float DXCore::AspectRatio() const
{
	return static_cast<float>(screenWidth) / screenHeight;
}

bool DXCore::Get4xMsaaState() const
{
	return xMsaaState;
}

void DXCore::Set4xMsaaStata(bool value)
{
	if (xMsaaState != value)
	{
		xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		Resize();
	}
}

int DXCore::Run()
{
	MSG msg = { 0 };

	//timer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			timer.UpdateTimer();

			if (!applicationPaused)
			{
				timer.UpdateTitleBarStats();
				Update(timer);
				Draw(timer);
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool DXCore::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	Resize();

	if (!InitImGui())
		return false;

	return true;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT DXCore::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

	switch (msg)
	{
		// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		screenWidth = LOWORD(lParam);
		screenHeight = HIWORD(lParam);
		if (Device)
		{
			if (wParam == SIZE_MINIMIZED)
			{
				applicationPaused = true;
				applicationMinimized = true;
				applicationMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				applicationPaused = false;
				applicationMinimized = false;
				applicationMaximized = true;
				Resize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				// Restoring from minimized state?
				if (applicationMinimized)
				{
					applicationPaused = false;
					applicationMinimized = false;
					Resize();
				}

				// Restoring from maximized state?
				else if (applicationMaximized)
				{
					applicationPaused = false;
					applicationMaximized = false;
					Resize();
				}
				else if (applicationResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					Resize();
				}
			}
		}
		return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
		// Don't beep when we alt-enter.
		return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	case WM_KEYDOWN:
	{
		unsigned char keyCode = static_cast<unsigned char>(wParam);
		if (InputManager::getInstance()->isKeysAutoRepeat())
		{
			InputManager::getInstance()->OnKeyPressed(keyCode);
		}
		else
		{
			const bool wasPressed = lParam & 0x40000000;
			if (!wasPressed)
			{
				InputManager::getInstance()->OnKeyPressed(keyCode);
			}
		}

		return 0;
	}

	case WM_KEYUP:
	{
		unsigned char keyCode = static_cast<unsigned char>(wParam);
		InputManager::getInstance()->OnKeyReleased(keyCode);
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}

		return 0;
	}
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		InputManager::getInstance()->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		InputManager::getInstance()->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_MOUSEMOVE:
		InputManager::getInstance()->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void DXCore::CreateRTVDSVCBVDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDescription;
	RTVHeapDescription.NumDescriptors = 8;
	RTVHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	RTVHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	RTVHeapDescription.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(
		&RTVHeapDescription, IID_PPV_ARGS(RTVHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDescription;
	DSVHeapDescription.NumDescriptors = 2;
	DSVHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	DSVHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DSVHeapDescription.NodeMask = 0;
	ThrowIfFailed(Device->CreateDescriptorHeap(
		&DSVHeapDescription, IID_PPV_ARGS(DSVHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
	uavHeapDesc.NumDescriptors = 23;
	uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(Device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&CBVSRVUAVHeap)));
	
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDesc.Count = 4;
	queryHeapDesc.NodeMask = 0;
	ThrowIfFailed(Device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&QueryHeap)));
}

void DXCore::Resize()
{
	assert(Device);
	assert(SwapChain);
	assert(GraphicsCommandListAllocator);

	// flush before changing any resources
	FlushCommandQueue();

	ThrowIfFailed(GraphicsCommandList->Reset(GraphicsCommandListAllocator.Get(), nullptr));

	// G Buffer
	{
		for (int i = 0; i < GBufferCount; ++i)
			GBuffer[i].Reset();

		for (int i = 0; i < 2; i++)
			AccumulationBuffer[i].Reset();

		for (int i = 0; i < 2; i++)
			BloomBuffer[i].Reset();

		D3D12_RESOURCE_DESC resourceDesc;
		ZeroMemory(&resourceDesc, sizeof(resourceDesc));
		resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		resourceDesc.Alignment = 0;
		resourceDesc.SampleDesc.Count = 1;
		resourceDesc.SampleDesc.Quality = 0;
		resourceDesc.MipLevels = 1;
		resourceDesc.DepthOrArraySize = 1;
		resourceDesc.Width = (UINT)screenWidth;
		resourceDesc.Height = (UINT)screenHeight;
		resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE clearVal;
		clearVal.Color[0] = 0;
		clearVal.Color[1] = 0;
		clearVal.Color[2] = 0;
		clearVal.Color[3] = 1;

		for (int i = 0; i < GBufferCount; i++) {
			resourceDesc.Format = GBufferFormats[i];
			clearVal.Format = GBufferFormats[i];

			ThrowIfFailed(Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&clearVal,
				IID_PPV_ARGS(GBuffer[i].GetAddressOf())));

			wchar_t formattedString[32];
			swprintf(formattedString, 32, L"GBuffer-%d", i);
			GBuffer[i]->SetName(formattedString);
		}

		resourceDesc.Format = AccumulationBufferFormat;
		for (int i = 0; i < 2; i++)
		{
			ThrowIfFailed(Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&clearVal,
				IID_PPV_ARGS(AccumulationBuffer[i].GetAddressOf())));
			wchar_t formattedString[32];
			swprintf(formattedString, 32, L"AccumulationBuffer-%d", i);
			AccumulationBuffer[i]->SetName(formattedString);

			ThrowIfFailed(Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				&clearVal,
				IID_PPV_ARGS(BloomBuffer[i].GetAddressOf())));
			
			swprintf(formattedString, 32, L"BloomBuffer-%d", i);
			BloomBuffer[i]->SetName(formattedString);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::G_BUFFER, RTVDescriptorSize);

		for (UINT i = 0; i < GBufferCount; i++)
		{
			Device->CreateRenderTargetView(GBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, RTVDescriptorSize);
		}

		rtvHeapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::ACCUMULATION_BUFFER, RTVDescriptorSize);
		for (int i = 0; i < 2; i++)
		{
			Device->CreateRenderTargetView(AccumulationBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, RTVDescriptorSize);
		}
		
		rtvHeapHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::BLOOM_BUFFER, RTVDescriptorSize);
		for (int i = 0; i < 2; i++)
		{
			Device->CreateRenderTargetView(BloomBuffer[i].Get(), nullptr, rtvHeapHandle);
			rtvHeapHandle.Offset(1, RTVDescriptorSize);
		}

		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::G_BUFFER, CBVSRVUAVDescriptorSize);
		D3D12_SHADER_RESOURCE_VIEW_DESC descSRV;

		ZeroMemory(&descSRV, sizeof(descSRV));
		descSRV.Texture2D.MipLevels = 1;
		descSRV.Texture2D.MostDetailedMip = 0;
		descSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		for (int i = 0; i < GBufferCount; i++) {
			descSRV.Format = GBufferFormats[i];
			Device->CreateShaderResourceView(GBuffer[i].Get(), &descSRV, hDescriptor);
			hDescriptor.Offset(1, CBVSRVUAVDescriptorSize);
		}

		descSRV.Format = AccumulationBufferFormat;
		hDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::ACCUMULATION_BUFFER, CBVSRVUAVDescriptorSize);
		for (int i = 0; i < 2; i++)
		{
			Device->CreateShaderResourceView(AccumulationBuffer[i].Get(), &descSRV, hDescriptor);
			hDescriptor.Offset(1, CBVSRVUAVDescriptorSize);
		}

		hDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::BLOOM_BUFFER, CBVSRVUAVDescriptorSize);
		for (int i = 0; i < 2; i++)
		{
			Device->CreateShaderResourceView(BloomBuffer[i].Get(), &descSRV, hDescriptor);
			hDescriptor.Offset(1, CBVSRVUAVDescriptorSize);
		}
	}

	// release the previous resources we will be recreating
	for (int i = 0; i < SwapChainBufferCount; ++i)
		SwapChainBuffer[i].Reset();
	DepthStencilBuffer.Reset();

	// resize the swap chain
	ThrowIfFailed(SwapChain->ResizeBuffers(
		SwapChainBufferCount,
		screenWidth, screenHeight,
		BackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	currentBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHeapHandle(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::SWAP_CHAIN, RTVDescriptorSize);
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		ThrowIfFailed(SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])));
		Device->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, RTVHeapHandle);
		SwapChainBuffer[i]->SetName(L"SwapChain");
		RTVHeapHandle.Offset(1, RTVDescriptorSize);
	}

	// create the depth/stencil buffer and view
	D3D12_RESOURCE_DESC depthStencilDescription;
	depthStencilDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDescription.Alignment = 0;
	depthStencilDescription.Width = screenWidth;
	depthStencilDescription.Height = screenHeight;
	depthStencilDescription.DepthOrArraySize = 1;
	depthStencilDescription.MipLevels = 1;

	depthStencilDescription.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDescription.SampleDesc.Count = xMsaaState ? 4 : 1;
	depthStencilDescription.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	depthStencilDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDescription.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;
	ThrowIfFailed(Device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDescription,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(DepthStencilBuffer.GetAddressOf())));
	DepthStencilBuffer->SetName(L"DepthStencilBuffer");

	// create descriptor to mip level 0 of entire resource using the format of the resource
	D3D12_DEPTH_STENCIL_VIEW_DESC DSVDescription;
	DSVDescription.Flags = D3D12_DSV_FLAG_NONE;
	DSVDescription.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	DSVDescription.Format = DepthStencilFormat;
	DSVDescription.Texture2D.MipSlice = 0;
	Device->CreateDepthStencilView(DepthStencilBuffer.Get(), &DSVDescription, DepthStencilView());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::DEPTH_BUFFER, CBVSRVUAVDescriptorSize);
	Device->CreateShaderResourceView(DepthStencilBuffer.Get(), &srvDesc, hDescriptor);

	// transition the resource from its initial state to be used as a depth buffer
	GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// execute the resize commands
	ThrowIfFailed(GraphicsCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { GraphicsCommandList.Get() };
	GraphicsCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wait until resize is complete
	FlushCommandQueue();

	// update the viewport transform to cover the client area
	ScreenViewPort.TopLeftX = 0;
	ScreenViewPort.TopLeftY = 0;
	ScreenViewPort.Width = static_cast<float>(screenWidth);
	ScreenViewPort.Height = static_cast<float>(screenHeight);
	ScreenViewPort.MinDepth = 0.0f;
	ScreenViewPort.MaxDepth = 1.0f;

	ScissorRect = { 0, 0, screenWidth, screenHeight };
}

bool DXCore::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = applicationInstanceHandle;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWindow";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, screenWidth, screenHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	mainWindowHandle = CreateWindow(L"MainWindow", MainWindowCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, applicationInstanceHandle, 0);
	if (!mainWindowHandle)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mainWindowHandle, SW_SHOW);
	UpdateWindow(mainWindowHandle);

	timer = Timer(mainWindowHandle, MainWindowCaption.c_str());

	return true;
}

bool DXCore::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG) 
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory)));

	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,             // default adapter
		D3D_FEATURE_LEVEL_11_0,
		IID_PPV_ARGS(&Device));

#if defined(DEGUG) || defined(_DEBUG)
	Device->QueryInterface(IID_PPV_ARGS(&InfoQueue));
	InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	//InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
#endif

	ThrowIfFailed(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&GraphicsFence)));

	ThrowIfFailed(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&ComputeFence)));

	RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	DSVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CBVSRVUAVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = BackBufferFormat;
	msQualityLevels.SampleCount = 4;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(Device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&msQualityLevels,
		sizeof(msQualityLevels)));

	xMsaaQuality = msQualityLevels.NumQualityLevels;
	assert(xMsaaQuality > 0 && "Unexpected MSAA quality level.");

	CreateCommandObjects();
	CreateSwapChain();
	CreateRTVDSVCBVDescriptorHeaps();

	return true;
}

bool DXCore::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(mainWindowHandle);
	ImGui_ImplDX12_Init(Device.Get(), SwapChainBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM, CBVSRVUAVHeap.Get(),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::IMGUI_TEXTURE, CBVSRVUAVDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::IMGUI_TEXTURE, CBVSRVUAVDescriptorSize));

	return true;
}

void DXCore::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&GraphicsCommandQueue)));
	GraphicsCommandQueue->SetName(L"GraphicsCommandQueue");

	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	ThrowIfFailed(Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&ComputeCommandQueue)));
	ComputeCommandQueue->SetName(L"ComputeCommandQueue");

	ThrowIfFailed(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(GraphicsCommandListAllocator.GetAddressOf())));
	GraphicsCommandListAllocator->SetName(L"GraphicsCommandListAllocator");

	ThrowIfFailed(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		IID_PPV_ARGS(ComputeCommandListAllocator.GetAddressOf())));
	ComputeCommandListAllocator->SetName(L"ComputeCommandListAllocator");

	ThrowIfFailed(Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		GraphicsCommandListAllocator.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(GraphicsCommandList.GetAddressOf())));
	GraphicsCommandList->SetName(L"GraphicsCommandList");

	ThrowIfFailed(Device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		ComputeCommandListAllocator.Get(), // Associated command allocator
		nullptr,                   // Initial PipelineStateObject
		IID_PPV_ARGS(ComputeCommandList.GetAddressOf())));
	ComputeCommandList->SetName(L"ComputeCommandList");
	
	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it, and it needs to be closed before
	// calling Reset.
	GraphicsCommandList->Close();
	ComputeCommandList->Close();
}

void DXCore::CreateSwapChain()
{
	// release the previous swapchain we will be recreating
	SwapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = screenWidth;
	sd.BufferDesc.Height = screenHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = BackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mainWindowHandle;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// Note: Swap chain uses queue to perform flush.
	ThrowIfFailed(DXGIFactory->CreateSwapChain(
		GraphicsCommandQueue.Get(),
		&sd,
		SwapChain.GetAddressOf()));
}

void DXCore::FlushCommandQueue()
{
	// advance the fence value to mark commands up to this fence point
	currentGraphicsFence++;
	currentComputeFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(GraphicsCommandQueue->Signal(GraphicsFence.Get(), currentGraphicsFence));
	ThrowIfFailed(ComputeCommandQueue->Signal(ComputeFence.Get(), currentComputeFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (GraphicsFence->GetCompletedValue() < currentGraphicsFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, FALSE, false, EVENT_ALL_ACCESS);

		// fire event when GPU hits current fence  
		ThrowIfFailed(GraphicsFence->SetEventOnCompletion(currentGraphicsFence, eventHandle));

		// wait until the GPU hits current fence event is fired
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	if (ComputeFence->GetCompletedValue() < currentComputeFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, FALSE, false, EVENT_ALL_ACCESS);

		// fire event when GPU hits current fence  
		ThrowIfFailed(ComputeFence->SetEventOnCompletion(currentComputeFence, eventHandle));

		// wait until the GPU hits current fence event is fired
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void DXCore::PrintInfoMessages()
{
#if defined(DEGUG) || defined(_DEBUG)
	UINT64 messageCount = InfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
	for (UINT64 i = 0; i < messageCount; ++i) {
		SIZE_T messageLength = 0;
		InfoQueue->GetMessage(i, nullptr, &messageLength);

		std::vector<char> messageData(messageLength);
		D3D12_MESSAGE* message = reinterpret_cast<D3D12_MESSAGE*>(messageData.data());
		InfoQueue->GetMessage(i, message, &messageLength);

		printf("D3D12 Message: %s\n", message->pDescription);
	}
	InfoQueue->ClearStoredMessages();
#endif
}

ID3D12Resource* DXCore::CurrentBackBuffer() const
{
	return SwapChainBuffer[currentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DXCore::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		RTVHeap->GetCPUDescriptorHandleForHeapStart(),
		currentBackBuffer,
		RTVDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE DXCore::DepthStencilView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		DSVHeap->GetCPUDescriptorHandleForHeapStart(),
		(int)DSVIndex::DEPTH_STENCIL_MAIN,
		DSVDescriptorSize);
}
