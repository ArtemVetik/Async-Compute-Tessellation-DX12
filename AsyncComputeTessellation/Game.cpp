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

	CommandList->SetGraphicsRootSignature(geoRootSignature.Get());
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
	D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
	uavHeapDesc.NumDescriptors = 6;
	uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(Device->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&CBVSRVUAVHeap)));
}

void Game::BuildRootSignature()
{
	// geo root signature
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
			IID_PPV_ARGS(geoRootSignature.GetAddressOf())));
	}
}

void Game::BuildShadersAndInputLayout()
{
	Shaders["OpaqueVS"] = d3dUtil::CompileShader(L"DefaultVS.hlsl", nullptr, "main", "vs_5_1");
	Shaders["OpaquePS"] = d3dUtil::CompileShader(L"DefaultPS.hlsl", nullptr, "main", "ps_5_1");

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
	geoOpaquePsoDesc.pRootSignature = geoRootSignature.Get();
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