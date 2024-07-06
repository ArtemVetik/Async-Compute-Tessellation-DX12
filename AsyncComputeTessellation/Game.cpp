#include "Game.h"

const int gNumberFrameResources = 3;

Game::Game(HINSTANCE hInstance) : DXCore(hInstance)
{
	mainCamera = new Camera(screenWidth, screenHeight);

	systemData = new SystemData();
	inputManager = new InputManager();
}

Game::~Game()
{
	if (Device != nullptr)
		FlushCommandQueue();

	delete systemData;
	systemData = 0;

	delete inputManager;
}

bool Game::Initialize()
{
	if (!DXCore::Initialize())
		return false;

	// reset the command list to prep for initialization commands
	ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

	BuildGeometry();
	BuildUAVs();
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

	ThrowIfFailed(CommandList->Reset(currentCommandListAllocator.Get(), PSOs["Opaque"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVSRVUAVHeap.Get() };
	CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	CommandList->RSSetViewports(1, &ScreenViewPort);
	CommandList->RSSetScissorRects(1, &ScissorRect);

	// indicate a state transition on the resource usage
	CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// clear the back buffer and depth buffer
	CommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
	CommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// specify the buffers we are going to render to
	CommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	CommandList->SetGraphicsRootSignature(opaqueRootSignature.Get());
	auto geoObjectCB = currentFrameResource->ObjectCB->Resource();

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	for (size_t i = 0; i < AllRitems.size(); ++i)
	{
		auto ri = AllRitems[i].get();

		CommandList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		CommandList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		CommandList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = geoObjectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		CommandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		CommandList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}

	bool changePso;
	ImGuiDraw(&changePso);
	
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

	if (changePso)
	{
		ThrowIfFailed(CommandList->Reset(CommandListAllocator.Get(), nullptr));

		BuildPSOs();

		// execute the initialization commands
		ThrowIfFailed(CommandList->Close());
		ID3D12CommandList* cmdsLists[] = { CommandList.Get() };
		CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

		FlushCommandQueue();
	}

	PrintInfoMessages();
}

void Game::ImGuiDraw(bool* changePso)
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

	*changePso = false;
	if (ImGui::Checkbox("Wireframe Mode", &imguiParams.WireframeMode))
		*changePso = true;

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	ImGui::End();

	ImGui::Render();
	CommandList->SetDescriptorHeaps(1, CBVSRVUAVHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), CommandList.Get());
}

void Game::UpdateMainPassCB(const Timer& timer)
{
	auto currObjectCB = currentFrameResource->ObjectCB.get();
	for (auto& e : AllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		//if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX view = XMLoadFloat4x4(&mainCamera->GetViewMatrix());
			XMMATRIX projection = XMLoadFloat4x4(&mainCamera->GetProjectionMatrix());

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.View, XMMatrixTranspose(view));
			XMStoreFloat4x4(&objConstants.Projection, XMMatrixTranspose(projection));
			objConstants.CamPosition = mainCamera->GetPosition();
			objConstants.AspectRatio = (float)screenWidth / screenHeight;

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void Game::BuildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(50.0f, 50.0f, 100, 100);

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = 0;
	gridSubmesh.BaseVertexLocation = 0;

	auto totalVertexCount = grid.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Position = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].UV = grid.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		CommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		CommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["grid"] = gridSubmesh;

	Geometries[geo->Name] = std::move(geo);

	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixScaling(5.0f, 1.0f, 5.0f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f));
	gridRitem->ObjCBIndex = 0;
	gridRitem->Geo = Geometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	AllRitems.push_back(std::move(gridRitem));
}

void Game::BuildUAVs()
{
	int vertexCount = 100;

	// Vertex Pool
	{

		UINT64 vertexPoolByteSize = sizeof(DirectX::XMFLOAT3) * vertexCount;
		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexPoolByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWVertexPool)));
		RWVertexPool->SetName(L"VertexPool");

		D3D12_UNORDERED_ACCESS_VIEW_DESC vertexPoolUAVDescription = {};

		vertexPoolUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		vertexPoolUAVDescription.Buffer.FirstElement = 0;
		vertexPoolUAVDescription.Buffer.NumElements = vertexCount;
		vertexPoolUAVDescription.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT3);
		vertexPoolUAVDescription.Buffer.CounterOffsetInBytes = 0;
		vertexPoolUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		D3D12_SHADER_RESOURCE_VIEW_DESC vertexPoolSRVDescription = {};
		vertexPoolSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		vertexPoolSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		vertexPoolSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		vertexPoolSRVDescription.Buffer.FirstElement = 0;
		vertexPoolSRVDescription.Buffer.NumElements = vertexCount;
		vertexPoolSRVDescription.Buffer.StructureByteStride = sizeof(DirectX::XMFLOAT3);

		VertexPoolCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 1, CBVSRVUAVDescriptorSize);
		VertexPoolGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 1, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWVertexPool.Get(), nullptr, &vertexPoolUAVDescription, VertexPoolCPUUAV);

		VertexPoolCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 4, CBVSRVUAVDescriptorSize);
		VertexPoolGPUSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 4, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWVertexPool.Get(), &vertexPoolSRVDescription, VertexPoolCPUSRV);
	}

	// Draw List
	{
		UINT64 drawListByteSize = sizeof(UINT) * vertexCount;
		UINT64 countBufferOffset = AlignForUavCounter(drawListByteSize);

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(countBufferOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWDrawList)));
		RWDrawList->SetName(L"DrawList");

		D3D12_UNORDERED_ACCESS_VIEW_DESC drawlistUAVDescription = {};
		drawlistUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		drawlistUAVDescription.Buffer.FirstElement = 0;
		drawlistUAVDescription.Buffer.NumElements = vertexCount;
		drawlistUAVDescription.Buffer.StructureByteStride = sizeof(UINT);
		drawlistUAVDescription.Buffer.CounterOffsetInBytes = countBufferOffset;
		drawlistUAVDescription.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		drawlistUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		DrawListCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 2, CBVSRVUAVDescriptorSize);
		DrawListGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 2, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWDrawList.Get(), RWDrawList.Get(), &drawlistUAVDescription, DrawListCPUUAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC drawListSRVDescription = {};
		drawListSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		drawListSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		drawListSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		drawListSRVDescription.Buffer.FirstElement = 0;
		drawListSRVDescription.Buffer.NumElements = vertexCount;
		drawListSRVDescription.Buffer.StructureByteStride = sizeof(UINT);

		DrawListCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 5, CBVSRVUAVDescriptorSize);
		DrawListGPUSRV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 5, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWDrawList.Get(), &drawListSRVDescription, DrawListCPUSRV);

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(countBufferOffset + sizeof(UINT)),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&DrawListUploadBuffer)));
		DrawListUploadBuffer->SetName(L"DrawListUploadBuffer");
	}

	// Draw Args
	{
		UINT64 drawArgsByteSize = (sizeof(unsigned int) * 9);
		UINT64 countBufferOffset = AlignForUavCounter(drawArgsByteSize);

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(countBufferOffset + sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWDrawArgs)));
		RWDrawArgs.Get()->SetName(L"DrawArgs");

		D3D12_UNORDERED_ACCESS_VIEW_DESC drawArgsUAVDescription = {};

		drawArgsUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		drawArgsUAVDescription.Buffer.FirstElement = 0;
		drawArgsUAVDescription.Buffer.NumElements = 9;
		drawArgsUAVDescription.Buffer.StructureByteStride = sizeof(unsigned int);
		drawArgsUAVDescription.Buffer.CounterOffsetInBytes = countBufferOffset;
		drawArgsUAVDescription.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		drawArgsUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		DrawArgsCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart(), 3, CBVSRVUAVDescriptorSize);
		DrawArgsGPUUAV = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), 3, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWDrawArgs.Get(), RWDrawArgs.Get(), &drawArgsUAVDescription, DrawArgsCPUUAV);
	}
}

void Game::BuildRootSignature()
{
	// opaque root signature
	{
		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[1];
		slotRootParameter[0].InitAsConstantBufferView(0);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter,
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
		CD3DX12_DESCRIPTOR_RANGE uavTable;
		uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[2];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(1, &uavTable);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter,
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
	D3D12_INDIRECT_ARGUMENT_DESC Args[1];
	Args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;

	D3D12_COMMAND_SIGNATURE_DESC particleCommandSingatureDescription = {};
	particleCommandSingatureDescription.ByteStride = 36;
	particleCommandSingatureDescription.NumArgumentDescs = 1;
	particleCommandSingatureDescription.pArgumentDescs = Args;

	ThrowIfFailed(Device->CreateCommandSignature(
		&particleCommandSingatureDescription,
		NULL,
		IID_PPV_ARGS(tessellationCommandSignature.GetAddressOf())));
}

void Game::BuildShadersAndInputLayout()
{
	Shaders["OpaqueVS"] = d3dUtil::CompileShader(L"DefaultVS.hlsl", nullptr, "main", "vs_5_1");
	Shaders["OpaquePS"] = d3dUtil::CompileShader(L"DefaultPS.hlsl", nullptr, "main", "ps_5_1");
	Shaders["TessellationUpdate"] = d3dUtil::CompileShader(L"TessellationUpdate.hlsl", nullptr, "main", "cs_5_1");
	Shaders["TessellationCopyDraw"] = d3dUtil::CompileShader(L"TessellationCopyDraw.hlsl", nullptr, "main", "cs_5_1");
	
	geoInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
	if (imguiParams.WireframeMode)
		geoOpaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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
		FrameResources.push_back(std::make_unique<FrameResource>(Device.Get(),
			1, 1, 1));
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