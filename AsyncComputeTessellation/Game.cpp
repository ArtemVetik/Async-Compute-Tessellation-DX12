#include "Game.h"

const int gNumberFrameResources = 3;

Game::Game(HINSTANCE hInstance) : DXCore(hInstance)
{
	mainCamera = new Camera(screenWidth, screenHeight);

	systemData = new SystemData();
	inputManager = new InputManager();
	bintree = nullptr;
	pingPongCounter = 0;
}

Game::~Game()
{
	if (Device != nullptr)
		FlushCommandQueue();

	delete systemData;
	systemData = 0;

	delete inputManager;
	delete mainCamera;

	if (bintree != nullptr)
		delete bintree;
}

bool Game::Initialize()
{
	if (!DXCore::Initialize())
		return false;

	bintree = new Bintree(Device.Get(), CommandList.Get());

	// reset the command list to prep for initialization commands
	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

	BuildUAVs();
	UploadBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildFrameResources();
	BuildPSOs();

	// execute the initialization commands
	ThrowIfFailed(CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	ThrowIfFailed(CommandListAllocator->Reset());

	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

	currentFrameResourceIndex = (currentFrameResourceIndex + 1) % gNumberFrameResources;
	currentFrameResource = FrameResources[currentFrameResourceIndex].get();

	UpdateMainPassCB(timer);

	ThrowIfFailed(CommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists1[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists1);

	// Wait for the work to finish.
	FlushCommandQueue();
}

void Game::Resize()
{
	DXCore::Resize();

	mainCamera->SetProjectionMatrix(screenWidth, screenHeight);

	if (bintree)
	{
		bintree->InitMesh(imguiParams.MeshMode);
		bintree->UpdateLodFactor(&imguiParams, std::max(screenWidth, screenHeight), mainCamera->GetFov());
	}
}

void Game::Update(const Timer& timer)
{
	mainCamera->Update();
	inputManager->UpdateController();

	// Cycle through the circular frame resource array.
	currentFrameResourceIndex = (currentFrameResourceIndex + 1) % gNumberFrameResources;
	currentFrameResource = FrameResources[currentFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (currentFrameResource->Fence != 0 && Fence->GetCompletedValue() < currentFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, FALSE, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(Fence->SetEventOnCompletion(currentFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateMainPassCB(timer);
}

void Game::Draw(const Timer& timer)
{
	auto currentCommandListAllocator = currentFrameResource->commandListAllocator;

	// reuse the memory associated with command recording
	// we can only reset when the associated command lists have finished execution on the GPU
	ThrowIfFailed(currentCommandListAllocator->Reset());

	ThrowIfFailed(CommandList->Reset(currentCommandListAllocator.Get(), nullptr));

	CommandList->SetPipelineState(PSOs["tessellationUpdate"].Get());
	CommandList->SetComputeRootSignature(tessellationComputeRootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVSRVUAVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	auto objectCB = currentFrameResource->ObjectCB->Resource();
	auto tessellationCB = currentFrameResource->TessellationCB->Resource();
	auto perFrameCB = currentFrameResource->PerFrameCB->Resource();
	CommandList->SetComputeRootConstantBufferView(0, objectCB->GetGPUVirtualAddress());
	CommandList->SetComputeRootConstantBufferView(1, tessellationCB->GetGPUVirtualAddress());
	CommandList->SetComputeRootConstantBufferView(2, perFrameCB->GetGPUVirtualAddress());

	CommandList->SetComputeRootDescriptorTable(3, MeshDataVertexGPUUAV);
	CommandList->SetComputeRootDescriptorTable(4, MeshDataIndexGPUUAV);
	CommandList->SetComputeRootDescriptorTable(5, DrawArgsGPUUAV);
	CommandList->SetComputeRootDescriptorTable(6 + pingPongCounter, SubdBufferInGPUUAV);
	CommandList->SetComputeRootDescriptorTable(7 - pingPongCounter, SubdBufferOutGPUUAV);
	CommandList->SetComputeRootDescriptorTable(8, SubdCounterGPUUAV);

	CommandList->SetPipelineState(PSOs["tessellationUpdate"].Get());
	CommandList->SetComputeRootSignature(tessellationComputeRootSignature.Get());
	CommandList->Dispatch(10000, 1, 1); // TODO: figure out how many threads group to run

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(RWSubdBufferIn.Get()));
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(RWSubdBufferOut.Get()));

	CommandList->SetPipelineState(PSOs["tessellationCopyDraw"].Get());
	CommandList->SetComputeRootSignature(tessellationComputeRootSignature.Get());
	CommandList->Dispatch(1, 1, 1);

	if (imguiParams.WireframeMode)
		CommandList->SetPipelineState(PSOs["Wireframe"].Get());
	else
		CommandList->SetPipelineState(PSOs["Opaque"].Get());

	CommandList->RSSetViewports(1, &ScreenViewPort);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	// indicate a state transition on the resource usage
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// clear the back buffer and depth buffer
	CommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Aqua, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// specify the buffers we are going to render to
	CommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	CommandList->SetGraphicsRootSignature(opaqueRootSignature.Get());

	CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//CommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	CommandList->SetGraphicsRootConstantBufferView(0, objectCB->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootConstantBufferView(1, tessellationCB->GetGPUVirtualAddress());
	CommandList->SetGraphicsRootConstantBufferView(2, perFrameCB->GetGPUVirtualAddress());

	CommandList->SetGraphicsRootDescriptorTable(3, MeshDataVertexGPUSRV);
	CommandList->SetGraphicsRootDescriptorTable(4, MeshDataIndexGPUSRV);
	if (pingPongCounter == 0)
		CommandList->SetGraphicsRootDescriptorTable(5, SubdBufferOutGPUSRV);
	else
		CommandList->SetGraphicsRootDescriptorTable(5, SubdBufferInGPUSRV);

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(RWDrawArgs.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

	CommandList->ExecuteIndirect(
		tessellationCommandSignature.Get(),
		1,
		RWDrawArgs.Get(),
		0,
		nullptr,
		0);

	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(RWDrawArgs.Get(),
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	ImguiOutput imguiOutput;
	ImGuiDraw(imguiOutput);

	// indicate a state transition on the resource usage
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// done recording commands
	ThrowIfFailed(CommandList->Close());
	// add the command list to the queue for execution
	ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
	CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// wwap the back and front buffers
	ThrowIfFailed(SwapChain->Present(0, 0));
	currentBackBuffer = (currentBackBuffer + 1) % SwapChainBufferCount;

	// advance the fence value to mark commands up to this fence point
	currentFrameResource->Fence = ++currentFence;

	// add an instruction to the command queue to set a new fence point.
	// because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal()
	CommandQueue->Signal(Fence.Get(), currentFence);

	if (imguiOutput.HasChanges())
	{
		FlushCommandQueue();
		ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

		if (imguiOutput.RebuildMesh)
			BuildUAVs();

		if (imguiOutput.ReuploadBuffers)
		{
			UploadBuffers();
			pingPongCounter = 1;
		}

		if (imguiOutput.RecompileShaders)
		{
			BuildShadersAndInputLayout();
			BuildPSOs();
		}

		// execute the initialization commands
		ThrowIfFailed(CommandList->Close());
		ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
		CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
		FlushCommandQueue();
	}

	pingPongCounter = 1 - pingPongCounter;
	PrintInfoMessages();
}

void Game::ImGuiDraw(ImguiOutput& output)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (imguiParams.ShowDebugWindow)
		ImGui::ShowDemoWindow(&imguiParams.ShowDebugWindow);

	static int counter = 0;
	ImGui::Begin("Tessellation parameters | TEST");
	ImGui::Text("Test application parameters.");
	ImGui::Checkbox("Demo Window", &imguiParams.ShowDebugWindow);

	output = {};

	if (ImGui::Combo("Mode", (int*)&imguiParams.MeshMode, "Terrain\0Mesh\0\0"))
	{
		output.RebuildMesh = true;
	}

	ImGui::Checkbox("Wireframe Mode", &imguiParams.WireframeMode);

	if (ImGui::SliderInt("CPU Lod Level", &imguiParams.CPULodLevel, 0, 4))
		output.ReuploadBuffers = true;

	if (ImGui::Checkbox("Uniform", &imguiParams.Uniform))
		output.RecompileShaders = true;

	if (imguiParams.Uniform)
	{
		if (ImGui::SliderInt("GPU Lod Level", &imguiParams.GPULodLevel, 0, 16))
			output.ReuploadBuffers = true;
	}

	if (imguiParams.MeshMode == MeshMode::TERRAIN)
	{
		if (ImGui::Checkbox("Displace Mapping", &imguiParams.UseDisplaceMapping))
			output.RecompileShaders = true;

		if (imguiParams.UseDisplaceMapping)
		{
			ImGui::SliderFloat("Displace Factor", &imguiParams.DisplaceFactor, 1, 20);
			ImGui::Checkbox("Waves Animation", &imguiParams.WavesAnimation);
			ImGui::SliderFloat("Displace Lacunarity", &imguiParams.DisplaceLacunarity, 0.7, 3);
			ImGui::SliderFloat("Displace PosScale", &imguiParams.DisplacePosScale, 0.01, 0.05);
			ImGui::SliderFloat("Displace H", &imguiParams.DisplaceH, 0.1, 2);
		}
	}

	float expo = log2(imguiParams.TargetLength);
	if (ImGui::SliderFloat("Edge Length (2^x)", &expo, 2, 10))
	{
		imguiParams.TargetLength = std::pow(2, expo);
		bintree->UpdateLodFactor(&imguiParams, std::max(screenWidth, screenHeight), mainCamera->GetFov());
	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::End();

	ImGui::Render();
	CommandList->SetDescriptorHeaps(1, CBVSRVUAVHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList.Get());
}

void Game::UpdateMainPassCB(const Timer& timer)
{
	// TODO: optimize loading of constant buffers (load only when needed, not every frame)

	auto currObjectCB = currentFrameResource->ObjectCB.get();
	XMMATRIX world = XMLoadFloat4x4(&MathHelper::Identity4x4());
	XMMATRIX view = XMLoadFloat4x4(&mainCamera->GetViewMatrix());
	XMMATRIX projection = XMLoadFloat4x4(&mainCamera->GetProjectionMatrix());

	ObjectConstants objConstants = {};
	XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&objConstants.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&objConstants.Projection, XMMatrixTranspose(projection));
	objConstants.AspectRatio = (float)screenWidth / screenHeight;
	currObjectCB->CopyData(0, objConstants);

	auto currTessellationCB = currentFrameResource->TessellationCB.get();
	TessellationConstants tessellationConstants = {};
	tessellationConstants.ScreenRes = std::max(screenWidth, screenHeight);
	XMStoreFloat4x4(&tessellationConstants.MeshWorld, XMMatrixTranspose(world));
	tessellationConstants.SubdivisionLevel = imguiParams.GPULodLevel;
	tessellationConstants.DisplaceFactor = imguiParams.DisplaceFactor;
	tessellationConstants.WavesAnimationFlag = imguiParams.WavesAnimation;
	tessellationConstants.DisplaceLacunarity = imguiParams.DisplaceLacunarity;
	tessellationConstants.DisplacePosScale = imguiParams.DisplacePosScale;
	tessellationConstants.DisplaceH = imguiParams.DisplaceH;
	tessellationConstants.LodFactor = imguiParams.LodFactor;
	currTessellationCB->CopyData(0, tessellationConstants);

	auto currFrameCB = currentFrameResource->PerFrameCB.get();
	PerFrameConstants perFrameConstants = {};
	perFrameConstants.CamPosition = mainCamera->GetPosition();
	perFrameConstants.DeltaTime = timer.GetDeltaTime();
	perFrameConstants.TotalTime = timer.GetTotalTime();
	currFrameCB->CopyData(0, perFrameConstants);
}

void Game::BuildUAVs()
{
	bintree->InitMesh(imguiParams.MeshMode);
	bintree->UpdateLodFactor(&imguiParams, std::max(screenWidth, screenHeight), mainCamera->GetFov());

	// Mesh Data Vertices
	{
		int vertexCount = bintree->GetMeshData().Vertices.size();
		UINT64 meshDataVertexByteSize = sizeof(DirectX::XMFLOAT3) * vertexCount;
		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(meshDataVertexByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWMeshDataVertex)));
		RWMeshDataVertex->SetName(L"MeshDataVertex");

		D3D12_UNORDERED_ACCESS_VIEW_DESC meshDataVertexUAVDescription = {};

		meshDataVertexUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		meshDataVertexUAVDescription.Buffer.FirstElement = 0;
		meshDataVertexUAVDescription.Buffer.NumElements = vertexCount;
		meshDataVertexUAVDescription.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT3);
		meshDataVertexUAVDescription.Buffer.CounterOffsetInBytes = 0;
		meshDataVertexUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		MeshDataVertexCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 1, CBVSRVUAVDescriptorSize);
		MeshDataVertexGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 1, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWMeshDataVertex.Get(), nullptr, &meshDataVertexUAVDescription, MeshDataVertexCPUUAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC meshDataVertexSRVDescription = {};
		meshDataVertexSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		meshDataVertexSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		meshDataVertexSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		meshDataVertexSRVDescription.Buffer.FirstElement = 0;
		meshDataVertexSRVDescription.Buffer.NumElements = vertexCount;
		meshDataVertexSRVDescription.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT3);

		MeshDataVertexCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 2, CBVSRVUAVDescriptorSize);
		MeshDataVertexGPUSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 2, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWMeshDataVertex.Get(), &meshDataVertexSRVDescription, MeshDataVertexCPUSRV);
	}

	// Mesh Data Indices
	{
		int indexCount = bintree->GetMeshData().Indices32.size();
		UINT64 meshDataVertexByteSize = sizeof(UINT) * indexCount;
		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(meshDataVertexByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWMeshDataIndex)));
		RWMeshDataIndex->SetName(L"MeshDataIndex");

		D3D12_UNORDERED_ACCESS_VIEW_DESC meshDataIndexUAVDescription = {};

		meshDataIndexUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		meshDataIndexUAVDescription.Buffer.FirstElement = 0;
		meshDataIndexUAVDescription.Buffer.NumElements = indexCount;
		meshDataIndexUAVDescription.Buffer.StructureByteStride = sizeof(UINT);
		meshDataIndexUAVDescription.Buffer.CounterOffsetInBytes = 0;
		meshDataIndexUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		MeshDataIndexCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 3, CBVSRVUAVDescriptorSize);
		MeshDataIndexGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 3, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWMeshDataIndex.Get(), nullptr, &meshDataIndexUAVDescription, MeshDataIndexCPUUAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC meshDataIndexSRVDescription = {};
		meshDataIndexSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		meshDataIndexSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		meshDataIndexSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		meshDataIndexSRVDescription.Buffer.FirstElement = 0;
		meshDataIndexSRVDescription.Buffer.NumElements = indexCount;
		meshDataIndexSRVDescription.Buffer.StructureByteStride = sizeof(UINT);

		MeshDataIndexCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 4, CBVSRVUAVDescriptorSize);
		MeshDataIndexGPUSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 4, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWMeshDataIndex.Get(), &meshDataIndexSRVDescription, MeshDataIndexCPUSRV);
	}

	// Draw Args
	{
		int drawArgsCount = sizeof(IndirectCommand) / sizeof(UINT);
		UINT64 drawArgsByteSize = (sizeof(unsigned int) * drawArgsCount);

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(drawArgsByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWDrawArgs)));
		RWDrawArgs.Get()->SetName(L"DrawArgs");

		D3D12_UNORDERED_ACCESS_VIEW_DESC drawArgsUAVDescription = {};

		drawArgsUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		drawArgsUAVDescription.Buffer.FirstElement = 0;
		drawArgsUAVDescription.Buffer.NumElements = drawArgsCount;
		drawArgsUAVDescription.Buffer.StructureByteStride = sizeof(unsigned int);
		drawArgsUAVDescription.Buffer.CounterOffsetInBytes = 0;
		drawArgsUAVDescription.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		drawArgsUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		DrawArgsCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 5, CBVSRVUAVDescriptorSize);
		DrawArgsGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 5, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWDrawArgs.Get(), nullptr, &drawArgsUAVDescription, DrawArgsCPUUAV);
	}

	// Subd Buffer In/Out
	{
		int subdSize = 10000000; // TODO: find out what size is needed here
		UINT64 subdBufferByteSize = sizeof(XMUINT4) * subdSize;

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(subdBufferByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWSubdBufferIn)));
		RWSubdBufferIn->SetName(L"SubdBufferIn");

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(subdBufferByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWSubdBufferOut)));
		RWSubdBufferOut->SetName(L"SubdBufferOut");

		D3D12_UNORDERED_ACCESS_VIEW_DESC subdBufferUAVDescription = {};

		subdBufferUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		subdBufferUAVDescription.Buffer.FirstElement = 0;
		subdBufferUAVDescription.Buffer.NumElements = subdSize;
		subdBufferUAVDescription.Buffer.StructureByteStride = sizeof(DirectX::XMUINT4);
		subdBufferUAVDescription.Buffer.CounterOffsetInBytes = 0;
		subdBufferUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		D3D12_SHADER_RESOURCE_VIEW_DESC subdBufferSRVDescription = {};
		subdBufferSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		subdBufferSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		subdBufferSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		subdBufferSRVDescription.Buffer.FirstElement = 0;
		subdBufferSRVDescription.Buffer.NumElements = subdSize;
		subdBufferSRVDescription.Buffer.StructureByteStride = sizeof(DirectX::XMUINT4);

		SubdBufferInCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 6, CBVSRVUAVDescriptorSize);
		SubdBufferInGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 6, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdBufferIn.Get(), nullptr, &subdBufferUAVDescription, SubdBufferInCPUUAV);

		SubdBufferOutCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 7, CBVSRVUAVDescriptorSize);
		SubdBufferOutGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 7, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdBufferOut.Get(), nullptr, &subdBufferUAVDescription, SubdBufferOutCPUUAV);

		SubdBufferInCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 8, CBVSRVUAVDescriptorSize);
		SubdBufferInGPUSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 8, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWSubdBufferIn.Get(), &subdBufferSRVDescription, SubdBufferInCPUSRV);

		SubdBufferOutCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 9, CBVSRVUAVDescriptorSize);
		SubdBufferOutGPUSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 9, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWSubdBufferOut.Get(), &subdBufferSRVDescription, SubdBufferOutCPUSRV);
	}

	// Subd Counter
	{
		UINT64 subdCounterByteSize = (sizeof(unsigned int) * 2);

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(subdCounterByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWSubdCounter)));
		RWSubdCounter.Get()->SetName(L"SubdCounter");

		D3D12_UNORDERED_ACCESS_VIEW_DESC subdCounterUAVDescription = {};

		subdCounterUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		subdCounterUAVDescription.Buffer.FirstElement = 0;
		subdCounterUAVDescription.Buffer.NumElements = 2;
		subdCounterUAVDescription.Buffer.StructureByteStride = sizeof(unsigned int);
		subdCounterUAVDescription.Buffer.CounterOffsetInBytes = 0;
		subdCounterUAVDescription.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		subdCounterUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		SubdCounterCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 10, CBVSRVUAVDescriptorSize);
		SubdCounterGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 10, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdCounter.Get(), 0, &subdCounterUAVDescription, SubdCounterCPUUAV);
	}
}

void Game::UploadBuffers()
{
	bintree->UploadMeshData(RWMeshDataVertex.Get(), RWMeshDataIndex.Get());
	bintree->UploadSubdivisionBuffer(RWSubdBufferIn.Get());
	bintree->UploadSubdivisionCounter(RWSubdCounter.Get());
	bintree->UploadDrawArgs(RWDrawArgs.Get(), imguiParams.CPULodLevel);
}

void Game::BuildRootSignature()
{
	// opaque root signature
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable0;
		srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE srvTable1;
		srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_DESCRIPTOR_RANGE srvTable2;
		srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[6];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &srvTable0);
		slotRootParameter[4].InitAsDescriptorTable(1, &srvTable1);
		slotRootParameter[5].InitAsDescriptorTable(1, &srvTable2);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
			(UINT)staticSamplers.size(),
			staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr)
		{
			::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		ThrowIfFailed(hr);

		ThrowIfFailed(Device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(opaqueRootSignature.GetAddressOf())));
	}

	// tessellation root signature
	{
		CD3DX12_DESCRIPTOR_RANGE uavTable0;
		uavTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE uavTable1;
		uavTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);

		CD3DX12_DESCRIPTOR_RANGE uavTable2;
		uavTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);

		CD3DX12_DESCRIPTOR_RANGE uavTable3;
		uavTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3);

		CD3DX12_DESCRIPTOR_RANGE uavTable4;
		uavTable4.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4);

		CD3DX12_DESCRIPTOR_RANGE uavTable5;
		uavTable5.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[9];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &uavTable0);
		slotRootParameter[4].InitAsDescriptorTable(1, &uavTable1);
		slotRootParameter[5].InitAsDescriptorTable(1, &uavTable2);
		slotRootParameter[6].InitAsDescriptorTable(1, &uavTable3);
		slotRootParameter[7].InitAsDescriptorTable(1, &uavTable4);
		slotRootParameter[8].InitAsDescriptorTable(1, &uavTable5);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(9, slotRootParameter,
			(UINT)staticSamplers.size(),
			staticSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
		ComPtr<ID3DBlob> serializedRootSig = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
			serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

		if (errorBlob != nullptr)
		{
			::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
		}
		ThrowIfFailed(hr);

		ThrowIfFailed(Device->CreateRootSignature(
			0,
			serializedRootSig->GetBufferPointer(),
			serializedRootSig->GetBufferSize(),
			IID_PPV_ARGS(tessellationComputeRootSignature.GetAddressOf())));
	}

	// tessellation command signature
	D3D12_INDIRECT_ARGUMENT_DESC Args[3];

	Args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
	Args[0].VertexBuffer.Slot = 0;
	Args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
	Args[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC particleCommandSingatureDescription = {};
	particleCommandSingatureDescription.ByteStride = sizeof(IndirectCommand);
	particleCommandSingatureDescription.NumArgumentDescs = _countof(Args);
	particleCommandSingatureDescription.pArgumentDescs = Args;

	ThrowIfFailed(Device->CreateCommandSignature(
		&particleCommandSingatureDescription,
		NULL,
		IID_PPV_ARGS(tessellationCommandSignature.GetAddressOf())));
}

void Game::BuildShadersAndInputLayout()
{
	D3D_SHADER_MACRO macros[] =
	{
		{"USE_DISPLACE", imguiParams.UseDisplaceMapping && imguiParams.MeshMode == MeshMode::TERRAIN ? "1" : "0"},
		{"UNIFORM_TESSELLATION", imguiParams.Uniform ? "1" : "0"},
		{NULL, NULL}
	};

	Shaders["OpaqueVS"] = d3dUtil::CompileShader(L"DefaultVS.hlsl", macros, "main", "vs_5_1");
	Shaders["OpaquePS"] = d3dUtil::CompileShader(L"DefaultPS.hlsl", macros, "main", "ps_5_1");
	Shaders["WireframeGS"] = d3dUtil::CompileShader(L"WireframeGS.hlsl", macros, "main", "gs_5_1");
	Shaders["WireframePS"] = d3dUtil::CompileShader(L"WireframePS.hlsl", macros, "main", "ps_5_1");
	Shaders["TessellationUpdate"] = d3dUtil::CompileShader(L"TessellationUpdate.hlsl", macros, "main", "cs_5_1");
	Shaders["TessellationCopyDraw"] = d3dUtil::CompileShader(L"TessellationCopyDraw.hlsl", macros, "main", "cs_5_1");

	geoInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Game::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC geoOpaquePsoDesc;
	ZeroMemory(&geoOpaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	geoOpaquePsoDesc.InputLayout = { geoInputLayout.data(), (UINT)geoInputLayout.size() };
	geoOpaquePsoDesc.pRootSignature = opaqueRootSignature.Get();
	geoOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["OpaqueVS"]->GetBufferPointer()),
		Shaders["OpaqueVS"]->GetBufferSize()
	};
	geoOpaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["OpaquePS"]->GetBufferPointer()),
		Shaders["OpaquePS"]->GetBufferSize()
	};
	geoOpaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	geoOpaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // TODO: use D3D12_CULL_MODE_FRONT (tessellation algorithm will need to be modified)
	geoOpaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	geoOpaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	geoOpaquePsoDesc.SampleMask = UINT_MAX;
	geoOpaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	geoOpaquePsoDesc.NumRenderTargets = 1;
	geoOpaquePsoDesc.RTVFormats[0] = BackBufferFormat;
	geoOpaquePsoDesc.SampleDesc.Count = xMsaaState ? 4 : 1;
	geoOpaquePsoDesc.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	geoOpaquePsoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&geoOpaquePsoDesc, IID_PPV_ARGS(&PSOs["Opaque"])));
	PSOs["Opaque"]->SetName(L"OpaquePSO");


	D3D12_GRAPHICS_PIPELINE_STATE_DESC geoWireframePsoDesc;
	ZeroMemory(&geoWireframePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	geoWireframePsoDesc.InputLayout = { geoInputLayout.data(), (UINT)geoInputLayout.size() };
	geoWireframePsoDesc.pRootSignature = opaqueRootSignature.Get();
	geoWireframePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["OpaqueVS"]->GetBufferPointer()),
		Shaders["OpaqueVS"]->GetBufferSize()
	};
	geoWireframePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["WireframePS"]->GetBufferPointer()),
		Shaders["WireframePS"]->GetBufferSize()
	};
	geoWireframePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(Shaders["WireframeGS"]->GetBufferPointer()),
		Shaders["WireframeGS"]->GetBufferSize()
	};
	geoWireframePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	geoWireframePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // TODO: use D3D12_CULL_MODE_FRONT (tessellation algorithm will need to be modified)
	geoWireframePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	geoWireframePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	geoWireframePsoDesc.SampleMask = UINT_MAX;
	geoWireframePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	geoWireframePsoDesc.NumRenderTargets = 1;
	geoWireframePsoDesc.RTVFormats[0] = BackBufferFormat;
	geoWireframePsoDesc.SampleDesc.Count = xMsaaState ? 4 : 1;
	geoWireframePsoDesc.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	geoWireframePsoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&geoWireframePsoDesc, IID_PPV_ARGS(&PSOs["Wireframe"])));
	PSOs["Wireframe"]->SetName(L"WireframePSO");


	D3D12_COMPUTE_PIPELINE_STATE_DESC tessellationUpdatePSO = {};
	tessellationUpdatePSO.pRootSignature = tessellationComputeRootSignature.Get();
	tessellationUpdatePSO.CS =
	{
		reinterpret_cast<BYTE*>(Shaders["TessellationUpdate"]->GetBufferPointer()),
		Shaders["TessellationUpdate"]->GetBufferSize()
	};
	tessellationUpdatePSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(Device->CreateComputePipelineState(&tessellationUpdatePSO, IID_PPV_ARGS(&PSOs["tessellationUpdate"])));
	PSOs["tessellationUpdate"]->SetName(L"tessellationUpdate");


	D3D12_COMPUTE_PIPELINE_STATE_DESC tessellationCopyDrawPSO = {};
	tessellationCopyDrawPSO.pRootSignature = tessellationComputeRootSignature.Get();
	tessellationCopyDrawPSO.CS =
	{
		reinterpret_cast<BYTE*>(Shaders["TessellationCopyDraw"]->GetBufferPointer()),
		Shaders["TessellationCopyDraw"]->GetBufferSize()
	};
	tessellationCopyDrawPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(Device->CreateComputePipelineState(&tessellationCopyDrawPSO, IID_PPV_ARGS(&PSOs["tessellationCopyDraw"])));
	PSOs["tessellationCopyDraw"]->SetName(L"tessellationCopyDraw");
}

void Game::BuildFrameResources()
{
	for (int i = 0; i < gNumberFrameResources; ++i)
	{
		FrameResources.push_back(std::make_unique<FrameResource>(Device.Get(), 1, 1, 1));
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Game::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}