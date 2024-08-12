#include "Game.h"

const int gNumberFrameResources = 1;

Game::Game(HINSTANCE hInstance) : DXCore(hInstance)
{
	mainCamera = new Camera(screenWidth, screenHeight);

	inputManager = new InputManager();
	bintree = nullptr;
	bloom = nullptr;
	pingPongCounter = 0;
	subdCulledBuffIdx = 0;
	mAccumBuffRTVIdx = 0;
	mBloomBuffRTVIdx = 0;

	mRenderType = RenderType::Direct;
}

Game::~Game()
{
	if (Device != nullptr)
		FlushCommandQueue();

	delete inputManager;
	delete mainCamera;

	if (bintree != nullptr)
		delete bintree;

	if (bloom != nullptr)
		delete bloom;
}

bool Game::Initialize()
{
	if (!DXCore::Initialize())
		return false;

	bintree = new Bintree(Device.Get(), GraphicsCommandList.Get());
	bloom = new Bloom(Device.Get(), GraphicsCommandList.Get());

	mShadowMap = std::make_unique<ShadowMap>(Device.Get(), 4096, 4096);

	// reset the command list to prep for initialization commands
	ThrowIfFailed(GraphicsCommandList->Reset(GraphicsCommandListAllocator.Get(), nullptr));

	BuildUAVs();
	UploadBuffers();
	BuildSSQuad();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildFrameResources();
	BuildPSOs();

	// execute the initialization commands
	ThrowIfFailed(GraphicsCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { GraphicsCommandList.Get() };
	GraphicsCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	ThrowIfFailed(GraphicsCommandListAllocator->Reset());

	ThrowIfFailed(GraphicsCommandList->Reset(GraphicsCommandListAllocator.Get(), nullptr));

	currentFrameResourceIndex = (currentFrameResourceIndex + 1) % gNumberFrameResources;
	currentFrameResource = FrameResources[currentFrameResourceIndex].get();

	UpdateMainPassCB(timer);

	ThrowIfFailed(GraphicsCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists1[] = { GraphicsCommandList.Get() };
	GraphicsCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists1);

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
	mainCamera->Update(timer);
	inputManager->UpdateController();

	currentFrameResourceIndex = (currentFrameResourceIndex + 1) % gNumberFrameResources;
	currentFrameResource = FrameResources[currentFrameResourceIndex].get();

	if (currentFrameResource->GraphicsFence != 0 && GraphicsFence->GetCompletedValue() < currentFrameResource->GraphicsFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, FALSE, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(GraphicsFence->SetEventOnCompletion(currentFrameResource->GraphicsFence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	if (currentFrameResource->ComputeFence != 0 && ComputeFence->GetCompletedValue() < currentFrameResource->ComputeFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, FALSE, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(ComputeFence->SetEventOnCompletion(currentFrameResource->ComputeFence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	//
	{
		imguiParams.CurrentComputeTime = GetQueryTimestamps(QueryResultBuffer[0].Get());
		imguiParams.CurrentTotalTime = GetQueryTimestamps(QueryResultBuffer[1].Get());
	}

	mLightRotationAngle += imguiParams.LightRotateSpeed * timer.GetDeltaTime();

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}

	UpdateShadowTransform(timer);
	UpdateMainPassCB(timer);
}

void Game::Draw(const Timer& timer)
{
	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVSRVUAVHeap.Get() };

	auto objectCB = currentFrameResource->ObjectCB->Resource();
	auto tessellationCB = currentFrameResource->TessellationCB->Resource();
	auto perFrameCB = currentFrameResource->PerFrameCB->Resource();
	auto lightPassCB = currentFrameResource->LightPassCB->Resource();

	auto depthBufferSrvDescGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::DEPTH_BUFFER, RTVDescriptorSize);

	ThrowIfFailed(currentFrameResource->graphicsCommandListAllocator->Reset());
	ThrowIfFailed(GraphicsCommandList->Reset(currentFrameResource->graphicsCommandListAllocator.Get(), nullptr));
	GraphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	GraphicsCommandList->EndQuery(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2);

	// compute pass
	{
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = GraphicsCommandList;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = currentFrameResource->graphicsCommandListAllocator;

		if (mRenderType != RenderType::Direct)
		{
			commandList = ComputeCommandList;
			commandAllocator = currentFrameResource->computeCommandListAllocator;

			ThrowIfFailed(commandAllocator->Reset());
			ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
			commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
		}
		else
		{
			subdCulledBuffIdx = 0;
		}

		commandList->EndQuery(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

		if (imguiParams.Freeze == false)
		{
			commandList->SetPipelineState(PSOs["tessellationUpdate"].Get());
			commandList->SetComputeRootSignature(tessellationComputeRootSignature.Get());

			commandList->SetComputeRootConstantBufferView(0, objectCB->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(1, tessellationCB->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(2, perFrameCB->GetGPUVirtualAddress());

			commandList->SetComputeRootDescriptorTable(3, GetSrvResourceDesc(CBVSRVUAVIndex::MESH_DATA_VERTEX_UAV));
			commandList->SetComputeRootDescriptorTable(4, GetSrvResourceDesc(CBVSRVUAVIndex::MESH_DATA_INDEX_UAV));
			commandList->SetComputeRootDescriptorTable(5, subdCulledBuffIdx == 0 ? GetSrvResourceDesc(CBVSRVUAVIndex::DRAW_ARGS_UAV_0) : GetSrvResourceDesc(CBVSRVUAVIndex::DRAW_ARGS_UAV_1));
			commandList->SetComputeRootDescriptorTable(6 + pingPongCounter, GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_IN_UAV));
			commandList->SetComputeRootDescriptorTable(7 - pingPongCounter, GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_UAV));
			commandList->SetComputeRootDescriptorTable(8, subdCulledBuffIdx == 0 ? GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_CULL_UAV_0) : GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_CULL_UAV_1));
			commandList->SetComputeRootDescriptorTable(9, GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_COUNTER_UAV));

			commandList->Dispatch(10000, 1, 1); // TODO: figure out how many threads group to run

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(RWSubdBufferIn.Get())); // TODO: are these lines necessary?
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(RWSubdBufferOut.Get()));
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(subdCulledBuffIdx == 0 ? RWSubdBufferOutCulled0.Get() : RWSubdBufferOutCulled1.Get()));

			commandList->SetPipelineState(PSOs["tessellationCopyDraw"].Get());
			commandList->SetComputeRootSignature(tessellationComputeRootSignature.Get());
			commandList->Dispatch(1, 1, 1);
		}

		commandList->EndQuery(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
		commandList->ResolveQueryData(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, QueryResultBuffer[0].Get(), 0);

		if (mRenderType != RenderType::Direct)
			ThrowIfFailed(commandList->Close());
	}

	if (mRenderType == RenderType::AsyncShadowMap)
		ExecuteComputeCommands(true);

	if (mRenderType == RenderType::Direct)
		subdCulledBuffIdx = 1;
	
	// shadow map pass
	{
		GraphicsCommandList->RSSetViewports(1, &mShadowMap->Viewport());
		GraphicsCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		GraphicsCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		GraphicsCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

		UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

		GraphicsCommandList->SetPipelineState(PSOs["ShadowOpaque"].Get());

		GraphicsCommandList->SetGraphicsRootSignature(opaqueRootSignature.Get());

		GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GraphicsCommandList->SetGraphicsRootConstantBufferView(0, objectCB->GetGPUVirtualAddress() + 1 * passCBByteSize);
		GraphicsCommandList->SetGraphicsRootConstantBufferView(1, tessellationCB->GetGPUVirtualAddress());
		GraphicsCommandList->SetGraphicsRootConstantBufferView(2, perFrameCB->GetGPUVirtualAddress());

		GraphicsCommandList->SetGraphicsRootDescriptorTable(3, GetSrvResourceDesc(CBVSRVUAVIndex::MESH_DATA_VERTEX_SRV));
		GraphicsCommandList->SetGraphicsRootDescriptorTable(4, GetSrvResourceDesc(CBVSRVUAVIndex::MESH_DATA_INDEX_SRV));
		GraphicsCommandList->SetGraphicsRootDescriptorTable(5, subdCulledBuffIdx == 0 ? GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_CULL_SRV_1) : GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_CULL_SRV_0));
		//CommandList->SetGraphicsRootDescriptorTable(6, mShadowMap->Srv());

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdCulledBuffIdx == 0 ?
			RWDrawArgs1.Get() : RWDrawArgs0.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

		GraphicsCommandList->ExecuteIndirect(
			tessellationCommandSignature.Get(),
			1,
			subdCulledBuffIdx == 0 ? RWDrawArgs1.Get() : RWDrawArgs0.Get(),
			0,
			nullptr,
			0);

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdCulledBuffIdx == 0 ?
			RWDrawArgs1.Get() : RWDrawArgs0.Get(),
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		// Change back to GENERIC_READ so we can read the texture in a shader.
		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
	}

	if (mRenderType == RenderType::AsyncShadowMap)
	{
		ThrowIfFailed(GraphicsCommandList->Close());
		
		ExecuteGraphicsCommands(false);
		
		GraphicsCommandQueue->Wait(ComputeFence.Get(), currentComputeFence);

		ResetGraphicsCommands();
	}

	// main draw pass
	{
		if (imguiParams.WireframeMode)
			GraphicsCommandList->SetPipelineState(PSOs["Wireframe"].Get());
		else
			GraphicsCommandList->SetPipelineState(PSOs["Opaque"].Get());

		GraphicsCommandList->RSSetViewports(1, &ScreenViewPort);
		GraphicsCommandList->RSSetScissorRects(1, &ScissorRect);

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		GraphicsCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Aqua, 0, nullptr);
		GraphicsCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDesc(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::G_BUFFER, RTVDescriptorSize);

		for (int i = 0; i < GBufferCount; i++) {
			GraphicsCommandList->ClearRenderTargetView(rtvDesc, DirectX::Colors::Black, 0, nullptr); // TODO: change color
			rtvDesc.Offset(1, RTVDescriptorSize);
		}

		rtvDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::G_BUFFER, RTVDescriptorSize);
		GraphicsCommandList->OMSetRenderTargets(GBufferCount, &rtvDesc, true, &DepthStencilView());

		GraphicsCommandList->SetGraphicsRootSignature(opaqueRootSignature.Get());

		GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GraphicsCommandList->SetGraphicsRootConstantBufferView(0, objectCB->GetGPUVirtualAddress());
		GraphicsCommandList->SetGraphicsRootConstantBufferView(1, tessellationCB->GetGPUVirtualAddress());
		GraphicsCommandList->SetGraphicsRootConstantBufferView(2, perFrameCB->GetGPUVirtualAddress());

		GraphicsCommandList->SetGraphicsRootDescriptorTable(3, GetSrvResourceDesc(CBVSRVUAVIndex::MESH_DATA_VERTEX_SRV));
		GraphicsCommandList->SetGraphicsRootDescriptorTable(4, GetSrvResourceDesc(CBVSRVUAVIndex::MESH_DATA_INDEX_SRV));
		GraphicsCommandList->SetGraphicsRootDescriptorTable(5, subdCulledBuffIdx == 0 ? GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_CULL_SRV_1) : GetSrvResourceDesc(CBVSRVUAVIndex::SUBD_OUT_CULL_SRV_0));
		GraphicsCommandList->SetGraphicsRootDescriptorTable(6, mShadowMap->Srv());
		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdCulledBuffIdx == 0 ?
			RWDrawArgs1.Get() : RWDrawArgs0.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

		GraphicsCommandList->ExecuteIndirect(
			tessellationCommandSignature.Get(),
			1,
			subdCulledBuffIdx == 0 ? RWDrawArgs1.Get() : RWDrawArgs0.Get(),
			0,
			nullptr,
			0);

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdCulledBuffIdx == 0 ?
			RWDrawArgs1.Get() : RWDrawArgs0.Get(),
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	}

	// light pass
	{
		GraphicsCommandList->OMSetRenderTargets(1, &GetAccumBufferRtvDesc(), true, nullptr);

		for (int i = 0; i < GBufferCount; i++)
		{
			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GBuffer[i].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
		}

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

		GraphicsCommandList->SetPipelineState(PSOs["DeferredLightPass"].Get());
		GraphicsCommandList->SetGraphicsRootSignature(gBufferRootSignature.Get());

		GraphicsCommandList->IASetVertexBuffers(0, 1, &ssQuadMesh->VertexBufferView());
		GraphicsCommandList->IASetIndexBuffer(&ssQuadMesh->IndexBufferView());
		GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GraphicsCommandList->SetGraphicsRootConstantBufferView(0, lightPassCB->GetGPUVirtualAddress());
		GraphicsCommandList->SetGraphicsRootDescriptorTable(1, GetSrvResourceDesc(CBVSRVUAVIndex::G_BUFFER));
		GraphicsCommandList->SetGraphicsRootDescriptorTable(2, depthBufferSrvDescGpu);

		GraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // TODO: !

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(AccumulationBuffer[mAccumBuffRTVIdx].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

		mAccumBuffRTVIdx = 1 - mAccumBuffRTVIdx;
	}

	if (mRenderType == RenderType::AsyncPostProcess)
	{
		ThrowIfFailed(GraphicsCommandList->Close());
		
		ExecuteGraphicsCommands(true);
		ResetGraphicsCommands();

		ComputeCommandQueue->Wait(GraphicsFence.Get(), currentGraphicsFence);

		ExecuteComputeCommands(true);
	}

	// bloom pass
	{
		auto bloomWeightsBufferSrvDescGpu = CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::BLOOM_WEIGHTS, RTVDescriptorSize);
		auto bloomPassCB = currentFrameResource->BloomCB->Resource();

		// bloom threshold pass
		{
			auto bloomBufferRtvDesc = CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::BLOOM_BUFFER + mBloomBuffRTVIdx, RTVDescriptorSize);

			GraphicsCommandList->OMSetRenderTargets(1, &bloomBufferRtvDesc, true, nullptr);

			GraphicsCommandList->SetPipelineState(PSOs["BloomThresholdPass"].Get());
			GraphicsCommandList->SetGraphicsRootSignature(bloomRootSignature.Get());

			GraphicsCommandList->IASetVertexBuffers(0, 1, &ssQuadMesh->VertexBufferView());
			GraphicsCommandList->IASetIndexBuffer(&ssQuadMesh->IndexBufferView());
			GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			GraphicsCommandList->SetGraphicsRootConstantBufferView(0, bloomPassCB->GetGPUVirtualAddress());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(1, GetAccumBufferSrvDesc());
			//CommandList->SetGraphicsRootDescriptorTable(2, bloomBuffer1SrvDescGpu);

			GraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // TODO: !

			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BloomBuffer[mBloomBuffRTVIdx].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

			mBloomBuffRTVIdx = 1 - mBloomBuffRTVIdx;
		}

		// TODO: make downsampling and upsampling
		// bloom H pass
		{
			GraphicsCommandList->OMSetRenderTargets(1, &GetBloomBufferRtvDesc(), true, nullptr);

			GraphicsCommandList->SetPipelineState(PSOs["BloomHPass"].Get());
			GraphicsCommandList->SetGraphicsRootSignature(bloomRootSignature.Get());

			GraphicsCommandList->IASetVertexBuffers(0, 1, &ssQuadMesh->VertexBufferView());
			GraphicsCommandList->IASetIndexBuffer(&ssQuadMesh->IndexBufferView());
			GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			GraphicsCommandList->SetGraphicsRootConstantBufferView(0, bloomPassCB->GetGPUVirtualAddress());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(1, GetAccumBufferSrvDesc());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(2, GetBloomBufferSrvDesc());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(3, bloomWeightsBufferSrvDescGpu);

			GraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // TODO: !

			mBloomBuffRTVIdx = 1 - mBloomBuffRTVIdx;
		}

		// bloom V pass
		{
			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BloomBuffer[mBloomBuffRTVIdx].Get(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

			GraphicsCommandList->OMSetRenderTargets(1, &GetBloomBufferRtvDesc(), true, nullptr);

			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BloomBuffer[(mBloomBuffRTVIdx + 1) % 2].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

			GraphicsCommandList->SetPipelineState(PSOs["BloomVPass"].Get());
			GraphicsCommandList->SetGraphicsRootSignature(bloomRootSignature.Get());

			GraphicsCommandList->IASetVertexBuffers(0, 1, &ssQuadMesh->VertexBufferView());
			GraphicsCommandList->IASetIndexBuffer(&ssQuadMesh->IndexBufferView());
			GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			GraphicsCommandList->SetGraphicsRootConstantBufferView(0, bloomPassCB->GetGPUVirtualAddress());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(1, GetAccumBufferSrvDesc());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(2, GetBloomBufferSrvDesc());
			GraphicsCommandList->SetGraphicsRootDescriptorTable(3, bloomWeightsBufferSrvDescGpu);

			GraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // TODO: !

			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BloomBuffer[mBloomBuffRTVIdx].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

			mBloomBuffRTVIdx = 1 - mBloomBuffRTVIdx;
		}
	}

	// motion blur pass
	{
		GraphicsCommandList->OMSetRenderTargets(1, &GetAccumBufferRtvDesc(), true, nullptr);

		GraphicsCommandList->SetPipelineState(PSOs["MotionBlurPass"].Get());
		GraphicsCommandList->SetGraphicsRootSignature(motionBlurRootSignature.Get());

		GraphicsCommandList->IASetVertexBuffers(0, 1, &ssQuadMesh->VertexBufferView());
		GraphicsCommandList->IASetIndexBuffer(&ssQuadMesh->IndexBufferView());
		GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GraphicsCommandList->SetGraphicsRootConstantBufferView(0, currentFrameResource->MotionBlurCB->Resource()->GetGPUVirtualAddress());
		GraphicsCommandList->SetGraphicsRootDescriptorTable(1, GetAccumBufferSrvDesc());
		GraphicsCommandList->SetGraphicsRootDescriptorTable(2, depthBufferSrvDescGpu);

		GraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // TODO: !

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(AccumulationBuffer[mAccumBuffRTVIdx].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

		mAccumBuffRTVIdx = 1 - mAccumBuffRTVIdx;
	}

	// final pass
	{
		GraphicsCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		GraphicsCommandList->SetPipelineState(PSOs["RenderQuadPass"].Get());
		GraphicsCommandList->SetGraphicsRootSignature(finalPassRootSignature.Get());

		GraphicsCommandList->IASetVertexBuffers(0, 1, &ssQuadMesh->VertexBufferView());
		GraphicsCommandList->IASetIndexBuffer(&ssQuadMesh->IndexBufferView());
		GraphicsCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		GraphicsCommandList->SetGraphicsRootConstantBufferView(0, lightPassCB->GetGPUVirtualAddress());
		GraphicsCommandList->SetGraphicsRootDescriptorTable(1, GetAccumBufferSrvDesc());
		GraphicsCommandList->SetGraphicsRootDescriptorTable(2, GetBloomBufferSrvDesc());

		GraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0); // TODO: !
	}

	ImguiOutput imguiOutput;
	RecordImGuiCommands(imguiOutput);

	// pre-render barriers
	{
		for (int i = 0; i < GBufferCount; i++)
		{
			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(GBuffer[i].Get(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		}

		for (int i = 0; i < 2; i++)
		{
			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(AccumulationBuffer[i].Get(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

			GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(BloomBuffer[i].Get(),
				D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));
		}

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(DepthStencilBuffer.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

		GraphicsCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}
	
	if (mRenderType == RenderType::AsyncAll)
		GraphicsCommandQueue->Wait(ComputeFence.Get(), currentComputeFence);

	GraphicsCommandList->EndQuery(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 3);
	GraphicsCommandList->ResolveQueryData(QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2, 2, QueryResultBuffer[1].Get(), 0);

	ThrowIfFailed(GraphicsCommandList->Close());
	ExecuteGraphicsCommands(true);

	if (mRenderType == RenderType::AsyncAll)
		ExecuteComputeCommands(true);

	ThrowIfFailed(SwapChain->Present(0, 0));
	currentBackBuffer = (currentBackBuffer + 1) % SwapChainBufferCount;

	if (imguiOutput.HasChanges())
	{
		FlushCommandQueue();
		ThrowIfFailed(GraphicsCommandList->Reset(GraphicsCommandListAllocator.Get(), nullptr));

		if (imguiOutput.RebuildMesh)
			BuildUAVs();

		if (imguiOutput.ReuploadBuffers || imguiOutput.RebuildMesh)
		{
			UploadBuffers();
			pingPongCounter = 1;
		}

		if (imguiOutput.RecompileShaders || imguiOutput.RebuildMesh)
		{
			BuildShadersAndInputLayout();
			BuildPSOs();
		}

		ThrowIfFailed(GraphicsCommandList->Close());
		ID3D12CommandList* cmdsLists[] = { GraphicsCommandList.Get() };
		GraphicsCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
		FlushCommandQueue();
	}

	subdCulledBuffIdx = 1 - subdCulledBuffIdx;
	pingPongCounter = 1 - pingPongCounter;
	PrintInfoMessages();
}

void Game::ExecuteGraphicsCommands(bool withSignal)
{
	ID3D12CommandList* cmdsLists[] = { GraphicsCommandList.Get() };
	GraphicsCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	if (withSignal)
	{
		currentFrameResource->GraphicsFence = ++currentGraphicsFence;
		GraphicsCommandQueue->Signal(GraphicsFence.Get(), currentGraphicsFence);
	}
}

void Game::ResetGraphicsCommands()
{
	ID3D12DescriptorHeap* descriptorHeaps[] = { CBVSRVUAVHeap.Get() };

	ThrowIfFailed(GraphicsCommandList->Reset(currentFrameResource->graphicsCommandListAllocator.Get(), nullptr));
	GraphicsCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	GraphicsCommandList->RSSetViewports(1, &ScreenViewPort);
	GraphicsCommandList->RSSetScissorRects(1, &ScissorRect);
}

void Game::ExecuteComputeCommands(bool withSignal)
{
	ID3D12CommandList* computeCmdsLists[] = { ComputeCommandList.Get() };
	ComputeCommandQueue->ExecuteCommandLists(_countof(computeCmdsLists), computeCmdsLists);

	currentFrameResource->ComputeFence = ++currentComputeFence;
	ComputeCommandQueue->Signal(ComputeFence.Get(), currentComputeFence);
}

void Game::RecordImGuiCommands(ImguiOutput& output)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (imguiParams.ShowDebugWindow)
		ImGui::ShowDemoWindow(&imguiParams.ShowDebugWindow);

	static int counter = 0;
	ImGui::Begin("App parameters | TEST");
	ImGui::Text("Test application parameters.");
	ImGui::Checkbox("Demo Window", &imguiParams.ShowDebugWindow);

	output = {};

	if (ImGui::Combo("Render Type", (int*)&imguiParams.RenderType, "Direct\0Async All\0Async Shadow Map\0Async Post Process\0\0"))
	{
		mRenderType = imguiParams.RenderType;
	}

	if (ImGui::CollapsingHeader("Tessellation parameters"))
	{
		ImGui::SeparatorText("View Mode");

		if (ImGui::Combo("Mode", (int*)&imguiParams.MeshMode, "Terrain\0Mesh\0\0"))
		{
			output.RebuildMesh = true;
		}


		ImGui::Checkbox("Wireframe Mode", &imguiParams.WireframeMode);

		if (imguiParams.WireframeMode == false)
		{
			if (ImGui::Checkbox("Flat Normals", &imguiParams.FlatNormals))
				output.RecompileShaders = true;
		}

		ImGui::SeparatorText("LoD");

		if (ImGui::SliderInt("CPU Lod Level", &imguiParams.CPULodLevel, 0, 4))
			output.ReuploadBuffers = true;

		if (ImGui::Checkbox("Uniform", &imguiParams.Uniform))
			output.RecompileShaders = true;

		if (imguiParams.Uniform)
		{
			ImGui::SameLine();
			if (ImGui::SliderInt(" ", &imguiParams.GPULodLevel, 0, 16))
				output.ReuploadBuffers = true;
		}

		float expo = log2(imguiParams.TargetLength);
		if (ImGui::SliderFloat("Edge Length (2^x)", &expo, 2, 10))
		{
			imguiParams.TargetLength = std::pow(2, expo);
			bintree->UpdateLodFactor(&imguiParams, std::max(screenWidth, screenHeight), mainCamera->GetFov());
		}

		if (imguiParams.MeshMode == MeshMode::TERRAIN)
		{
			ImGui::SeparatorText("Displace");

			if (ImGui::Checkbox("Displace Mapping", &imguiParams.UseDisplaceMapping))
				output.RecompileShaders = true;

			if (imguiParams.UseDisplaceMapping)
			{
				ImGui::SliderFloat("Displace Factor", &imguiParams.DisplaceFactor, 1, 20);
				ImGui::Checkbox("Animated", &imguiParams.WavesAnimation);
				ImGui::SliderFloat("Displace Lacunarity", &imguiParams.DisplaceLacunarity, 0.7, 3);
				ImGui::SliderFloat("Displace PosScale", &imguiParams.DisplacePosScale, 0.01, 0.05);
				ImGui::SliderFloat("Displace H", &imguiParams.DisplaceH, 0.1, 2);
			}
		}

		ImGui::SeparatorText("Compute Settings");

		ImGui::Checkbox("Freeze", &imguiParams.Freeze);
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		ImGui::SeparatorText("Directional Light");

		if (ImGui::SliderInt("Count", &imguiParams.DirectionalLightCount, 1, 3))
			output.RecompileShaders = true;

		for (int i = 0; i < imguiParams.DirectionalLightCount; i++)
		{
			char str[50];
			sprintf_s(str, "Color %d", i);

			ImGui::ColorEdit3(str, imguiParams.DLColor[i]);

			sprintf_s(str, "Intensivity %d", i);
			ImGui::InputFloat(str, &imguiParams.DLIntensivity[i]);
		}

		ImGui::Spacing();
		ImGui::SliderFloat("Rotate Speed", &imguiParams.LightRotateSpeed, 0, 5);

		ImGui::SeparatorText("Shading Parameters");

		ImGui::SliderFloat4("Diffuse Albedo", imguiParams.DiffuseAlbedo, 0, 1);
		ImGui::SliderFloat4("Ambient Light", imguiParams.AmbientLight, 0, 1);
		ImGui::SliderFloat("Roughness", &imguiParams.Roughness, 0, 1);
		ImGui::SliderFloat3("FresnelR0", imguiParams.FresnelR0, 0, 1);
	}

	if (ImGui::CollapsingHeader("Motion Blur"))
	{
		ImGui::SliderFloat("Motion Blur Amount", &imguiParams.MotionBlurAmount, 1, 100);
		ImGui::SliderInt("Motion Blur Sampler", &imguiParams.MotionBlurSamplerCount, 1, 50);
	}

	if (ImGui::CollapsingHeader("Bloom"))
	{
		ImGui::SliderFloat("Threshold", &imguiParams.Threshold, 0, 2);

		if (ImGui::SliderInt("Kernel Size", &imguiParams.BloomKernelSize, 3, 32))
		{
			output.RecompileShaders = true;
			output.ReuploadBuffers = true;
		}
	}

	ImGui::Checkbox("Show Stats", &imguiParams.ShowStats);

	if (imguiParams.ShowStats)
	{
		ImGui::Begin("Stats");

		if (imguiParams.PlotRefreshTime == 0)
			imguiParams.PlotRefreshTime = ImGui::GetTime();

		while (imguiParams.PlotRefreshTime < ImGui::GetTime())
		{
			imguiParams.ComputeTime[imguiParams.StatsOffset] = imguiParams.CurrentComputeTime;
			imguiParams.TotalTime[imguiParams.StatsOffset] = imguiParams.CurrentTotalTime;

			imguiParams.StatsOffset = (imguiParams.StatsOffset + 1) % imguiParams.PlotDataCount;
			imguiParams.PlotRefreshTime += 1.0f / 30.0f;
		}

		auto computeMax = *std::max_element(imguiParams.ComputeTime, imguiParams.ComputeTime + imguiParams.PlotDataCount);
		ImGui::PlotLines("GPU compute dT", imguiParams.ComputeTime,
			imguiParams.PlotDataCount, imguiParams.StatsOffset,
			std::to_string(imguiParams.CurrentComputeTime).c_str(),
			0.0f, computeMax, ImVec2(0, imguiParams.PlotDataCount));

		auto totalMax = *std::max_element(imguiParams.TotalTime, imguiParams.TotalTime + imguiParams.PlotDataCount);
		ImGui::PlotLines("GPU render dT", imguiParams.TotalTime,
			imguiParams.PlotDataCount, imguiParams.StatsOffset,
			std::to_string(imguiParams.CurrentTotalTime).c_str(),
			0.0f, computeMax, ImVec2(0, imguiParams.PlotDataCount));

		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::End();
	}

	ImGui::End();

	ImGui::Render();
	GraphicsCommandList->SetDescriptorHeaps(1, CBVSRVUAVHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GraphicsCommandList.Get());
}

void Game::UpdateMainPassCB(const Timer& timer)
{
	// TODO: optimize loading of constant buffers (load only when needed, not every frame)

	XMMATRIX world = XMLoadFloat4x4(&MathHelper::Identity4x4());
	XMMATRIX prevView = XMLoadFloat4x4(&mainCamera->GetPrevViewMatrix());
	XMMATRIX view = XMLoadFloat4x4(&mainCamera->GetViewMatrix());
	XMMATRIX projection = XMLoadFloat4x4(&mainCamera->GetProjectionMatrix());
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	ObjectConstants objConstants = {};
	XMStoreFloat4x4(&objConstants.World, DirectX::XMMatrixTranspose(world));
	XMStoreFloat4x4(&objConstants.View, DirectX::XMMatrixTranspose(view));
	XMStoreFloat4x4(&objConstants.Projection, DirectX::XMMatrixTranspose(projection));
	XMStoreFloat4x4(&objConstants.ShadowTransform, XMMatrixTranspose(shadowTransform));
	objConstants.AspectRatio = (float)screenWidth / screenHeight;

	FrustrumPlanes frustrum = mainCamera->GetPredictedFrustrumPlanes(world);

	for (int i = 0; i < 6; i++)
		objConstants.FrustrumPlanes[i] = frustrum.Planes[i];

	auto currObjectCB = currentFrameResource->ObjectCB.get();
	currObjectCB->CopyData(0, objConstants);

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
	auto currTessellationCB = currentFrameResource->TessellationCB.get();
	currTessellationCB->CopyData(0, tessellationConstants);

	PerFrameConstants perFrameConstants = {};
	perFrameConstants.CamPosition = mainCamera->GetPosition();
	perFrameConstants.PredictedCamPosition = mainCamera->GetPredictedPosition();
	perFrameConstants.DeltaTime = timer.GetDeltaTime();
	perFrameConstants.TotalTime = timer.GetTotalTime();
	auto currFrameCB = currentFrameResource->PerFrameCB.get();
	currFrameCB->CopyData(0, perFrameConstants);

	ObjectConstants shadowConstants = {};
	XMMATRIX lightView = XMLoadFloat4x4(&mLightView);
	XMMATRIX lightProjection = XMLoadFloat4x4(&mLightProj);
	XMStoreFloat4x4(&shadowConstants.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&shadowConstants.View, XMMatrixTranspose(lightView));
	XMStoreFloat4x4(&shadowConstants.Projection, XMMatrixTranspose(lightProjection));
	auto currShadowCB = currentFrameResource->ObjectCB.get();
	currShadowCB->CopyData(1, shadowConstants);

	LightPassConstants lightPassConstants = {};
	DirectX::XMStoreFloat4x4(&lightPassConstants.ViewInv, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mainCamera->GetViewMatrix())));
	DirectX::XMStoreFloat4x4(&lightPassConstants.ProjInv, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mainCamera->GetProjectionMatrix())));
	lightPassConstants.DiffuseAlbedo = DirectX::XMFLOAT4(imguiParams.DiffuseAlbedo);
	lightPassConstants.AmbientLight = DirectX::XMFLOAT4(imguiParams.AmbientLight);
	lightPassConstants.EyePosW = mainCamera->GetPosition();
	lightPassConstants.Roughness = imguiParams.Roughness;
	lightPassConstants.FresnelR0 = DirectX::XMFLOAT3(imguiParams.FresnelR0);
	lightPassConstants.Lights[0].Direction = mRotatedLightDirections[0];
	lightPassConstants.Lights[0].Strength = MathHelper::MultiplyFloat3(DirectX::XMFLOAT3(imguiParams.DLColor[0]), imguiParams.DLIntensivity[0]);
	lightPassConstants.Lights[1].Direction = mRotatedLightDirections[1];
	lightPassConstants.Lights[1].Strength = MathHelper::MultiplyFloat3(DirectX::XMFLOAT3(imguiParams.DLColor[1]), imguiParams.DLIntensivity[1]);
	lightPassConstants.Lights[2].Direction = mRotatedLightDirections[2];
	lightPassConstants.Lights[2].Strength = MathHelper::MultiplyFloat3(DirectX::XMFLOAT3(imguiParams.DLColor[2]), imguiParams.DLIntensivity[2]);
	auto lightPassCB = currentFrameResource->LightPassCB.get();
	lightPassCB->CopyData(0, lightPassConstants);

	MotionBlurConstants motionBlurConstants = {};
	XMStoreFloat4x4(&motionBlurConstants.ViewProj, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(view, projection)));
	XMStoreFloat4x4(&motionBlurConstants.PrevViewProj, DirectX::XMMatrixTranspose(DirectX::XMMatrixMultiply(prevView, projection)));
	DirectX::XMStoreFloat4x4(&motionBlurConstants.ViewInv, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mainCamera->GetViewMatrix())));
	DirectX::XMStoreFloat4x4(&motionBlurConstants.ProjInv, XMMatrixInverse(nullptr, XMLoadFloat4x4(&mainCamera->GetProjectionMatrix())));
	motionBlurConstants.BlureAmount = imguiParams.MotionBlurAmount;
	motionBlurConstants.SamplerCount = imguiParams.MotionBlurSamplerCount;
	auto motionBlurCB = currentFrameResource->MotionBlurCB.get();
	motionBlurCB->CopyData(0, motionBlurConstants);

	BloomConstants bloomConstants = {};
	bloomConstants.Threshold = imguiParams.Threshold;
	auto bloomCB = currentFrameResource->BloomCB.get();
	bloomCB->CopyData(0, bloomConstants);
}

void Game::UpdateShadowTransform(const Timer& timer)
{
	float radius = 300;

	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * radius * lightDir;
	XMVECTOR targetPos = { 0, 0, 0 };
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - radius;
	float b = sphereCenterLS.y - radius;
	float n = sphereCenterLS.z - radius;
	float r = sphereCenterLS.x + radius;
	float t = sphereCenterLS.y + radius;
	float f = sphereCenterLS.z + radius;

	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = lightView * lightProj * T;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void Game::BuildUAVs()
{
	bintree->InitMesh(imguiParams.MeshMode);
	bintree->UpdateLodFactor(&imguiParams, std::max(screenWidth, screenHeight), mainCamera->GetFov());

	auto srvCpuStart = CBVSRVUAVHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart();
	auto dsvCpuStart = DSVHeap->GetCPUDescriptorHandleForHeapStart();

	// Mesh Data Vertices
	{
		int vertexCount = bintree->GetMeshData().Vertices.size();
		UINT64 meshDataVertexByteSize = sizeof(Vertex) * vertexCount;
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
		meshDataVertexUAVDescription.Buffer.StructureByteStride = sizeof(Vertex);
		meshDataVertexUAVDescription.Buffer.CounterOffsetInBytes = 0;
		meshDataVertexUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		auto meshDataVertexCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::MESH_DATA_VERTEX_UAV, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWMeshDataVertex.Get(), nullptr, &meshDataVertexUAVDescription, meshDataVertexCPUUAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC meshDataVertexSRVDescription = {};
		meshDataVertexSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		meshDataVertexSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		meshDataVertexSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		meshDataVertexSRVDescription.Buffer.FirstElement = 0;
		meshDataVertexSRVDescription.Buffer.NumElements = vertexCount;
		meshDataVertexSRVDescription.Buffer.StructureByteStride = sizeof(Vertex);

		auto meshDataVertexCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::MESH_DATA_VERTEX_SRV, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWMeshDataVertex.Get(), &meshDataVertexSRVDescription, meshDataVertexCPUSRV);
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

		auto meshDataIndexCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::MESH_DATA_INDEX_UAV, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWMeshDataIndex.Get(), nullptr, &meshDataIndexUAVDescription, meshDataIndexCPUUAV);

		D3D12_SHADER_RESOURCE_VIEW_DESC meshDataIndexSRVDescription = {};
		meshDataIndexSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		meshDataIndexSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		meshDataIndexSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		meshDataIndexSRVDescription.Buffer.FirstElement = 0;
		meshDataIndexSRVDescription.Buffer.NumElements = indexCount;
		meshDataIndexSRVDescription.Buffer.StructureByteStride = sizeof(UINT);

		auto meshDataIndexCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::MESH_DATA_INDEX_SRV, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWMeshDataIndex.Get(), &meshDataIndexSRVDescription, meshDataIndexCPUSRV);
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
			IID_PPV_ARGS(&RWDrawArgs0)));
		RWDrawArgs0.Get()->SetName(L"DrawArgs0");

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(drawArgsByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWDrawArgs1)));
		RWDrawArgs1.Get()->SetName(L"DrawArgs1");

		D3D12_UNORDERED_ACCESS_VIEW_DESC drawArgsUAVDescription = {};

		drawArgsUAVDescription.Format = DXGI_FORMAT_UNKNOWN;
		drawArgsUAVDescription.Buffer.FirstElement = 0;
		drawArgsUAVDescription.Buffer.NumElements = drawArgsCount;
		drawArgsUAVDescription.Buffer.StructureByteStride = sizeof(unsigned int);
		drawArgsUAVDescription.Buffer.CounterOffsetInBytes = 0;
		drawArgsUAVDescription.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		drawArgsUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		auto drawArgsCPUUAV0 = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::DRAW_ARGS_UAV_0, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWDrawArgs0.Get(), nullptr, &drawArgsUAVDescription, drawArgsCPUUAV0);

		auto drawArgsCPUUAV1 = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::DRAW_ARGS_UAV_1, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWDrawArgs1.Get(), nullptr, &drawArgsUAVDescription, drawArgsCPUUAV1);
	}

	// Subd Buffer In/Out
	{
		int subdSize = 1000000; // TODO: find out what size is needed here
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

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(subdBufferByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWSubdBufferOutCulled0)));
		RWSubdBufferOutCulled0->SetName(L"SubdBufferOutCulled0");

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(subdBufferByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWSubdBufferOutCulled1)));
		RWSubdBufferOutCulled1->SetName(L"SubdBufferOutCulled1");

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

		auto subdBufferInCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_IN_UAV, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdBufferIn.Get(), nullptr, &subdBufferUAVDescription, subdBufferInCPUUAV);

		auto subdBufferOutCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_OUT_UAV, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdBufferOut.Get(), nullptr, &subdBufferUAVDescription, subdBufferOutCPUUAV);

		auto subdBufferOutCulledCPUUAV0 = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_OUT_CULL_UAV_0, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdBufferOutCulled0.Get(), nullptr, &subdBufferUAVDescription, subdBufferOutCulledCPUUAV0);

		auto subdBufferOutCulledCPUSRV0 = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_OUT_CULL_SRV_0, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWSubdBufferOutCulled0.Get(), &subdBufferSRVDescription, subdBufferOutCulledCPUSRV0);

		auto subdBufferOutCulledCPUUAV1 = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_OUT_CULL_UAV_1, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdBufferOutCulled1.Get(), nullptr, &subdBufferUAVDescription, subdBufferOutCulledCPUUAV1);

		auto subdBufferOutCulledCPUSRV1 = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_OUT_CULL_SRV_1, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWSubdBufferOutCulled1.Get(), &subdBufferSRVDescription, subdBufferOutCulledCPUSRV1);
	}

	// Subd Counter
	{
		UINT64 subdCounterByteSize = (sizeof(unsigned int) * 3);

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
		subdCounterUAVDescription.Buffer.NumElements = 3;
		subdCounterUAVDescription.Buffer.StructureByteStride = sizeof(unsigned int);
		subdCounterUAVDescription.Buffer.CounterOffsetInBytes = 0;
		subdCounterUAVDescription.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
		subdCounterUAVDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		auto subdCounterCPUUAV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SUBD_COUNTER_UAV, CBVSRVUAVDescriptorSize);
		Device->CreateUnorderedAccessView(RWSubdCounter.Get(), 0, &subdCounterUAVDescription, subdCounterCPUUAV);
	}

	// Shadow Maps
	{
		mShadowMap->BuildDescriptors(
			CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::SHADOW_MAP_SRV, CBVSRVUAVDescriptorSize),
			CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, (int)CBVSRVUAVIndex::SHADOW_MAP_SRV, CBVSRVUAVDescriptorSize),
			CD3DX12_CPU_DESCRIPTOR_HANDLE(dsvCpuStart, (int)DSVIndex::SHADOW_MAP_DEPTH, DSVDescriptorSize));
	}

	// Bloom Weights
	{
		int weightCount = 7;
		UINT64 bloomByteSize = sizeof(float) * weightCount;

		ThrowIfFailed(Device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bloomByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&RWBloomWeights)));
		RWBloomWeights.Get()->SetName(L"BloomWeights");

		D3D12_SHADER_RESOURCE_VIEW_DESC bloomWeightsSRVDescription = {};
		bloomWeightsSRVDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		bloomWeightsSRVDescription.Format = DXGI_FORMAT_UNKNOWN;
		bloomWeightsSRVDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		bloomWeightsSRVDescription.Buffer.FirstElement = 0;
		bloomWeightsSRVDescription.Buffer.NumElements = weightCount;
		bloomWeightsSRVDescription.Buffer.StructureByteStride = sizeof(float);

		auto BloomWeightsCPUSRV = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, (int)CBVSRVUAVIndex::BLOOM_WEIGHTS, CBVSRVUAVDescriptorSize);
		Device->CreateShaderResourceView(RWBloomWeights.Get(), &bloomWeightsSRVDescription, BloomWeightsCPUSRV);
	}

	// Query result buffer
	{
		D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(4 * sizeof(UINT64));
		
		for (int i = 0; i < 2; i++)
		{
			Device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
				D3D12_HEAP_FLAG_NONE,
				&resourceDesc,
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&QueryResultBuffer[i]));
		}
	}
}

void Game::UploadBuffers()
{
	bintree->UploadMeshData(RWMeshDataVertex.Get(), RWMeshDataIndex.Get());
	bintree->UploadSubdivisionBuffer(RWSubdBufferIn.Get());
	bintree->UploadSubdivisionCounter(RWSubdCounter.Get());
	bintree->UploadDrawArgs(RWDrawArgs0.Get(), RWDrawArgs1.Get(), imguiParams.CPULodLevel);
	bloom->UploadWeightsBuffer(RWBloomWeights.Get(), imguiParams.BloomKernelSize);
}

void Game::BuildSSQuad()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f);

	std::vector<VertexPT> vertices;
	std::vector<std::uint16_t> indices;

	for (size_t i = 0; i < quad.Vertices.size(); i++)
		vertices.push_back({ quad.Vertices[i].Position, quad.Vertices[i].TexC });

	for (size_t i = 0; i < quad.Indices32.size(); i++)
		indices.push_back(quad.Indices32[i]);

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(VertexPT);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	ssQuadMesh = std::make_unique<MeshGeometry>();
	ssQuadMesh->Name = "quad";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &ssQuadMesh->VertexBufferCPU));
	CopyMemory(ssQuadMesh->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &ssQuadMesh->IndexBufferCPU));
	CopyMemory(ssQuadMesh->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	ssQuadMesh->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		GraphicsCommandList.Get(), vertices.data(), vbByteSize, ssQuadMesh->VertexBufferUploader);

	ssQuadMesh->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(Device.Get(),
		GraphicsCommandList.Get(), indices.data(), ibByteSize, ssQuadMesh->IndexBufferUploader);

	ssQuadMesh->VertexByteStride = sizeof(VertexPT);
	ssQuadMesh->VertexBufferByteSize = vbByteSize;
	ssQuadMesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	ssQuadMesh->IndexBufferByteSize = ibByteSize;
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

		CD3DX12_DESCRIPTOR_RANGE srvTable3;
		srvTable3.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[7];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &srvTable0);
		slotRootParameter[4].InitAsDescriptorTable(1, &srvTable1);
		slotRootParameter[5].InitAsDescriptorTable(1, &srvTable2);
		slotRootParameter[6].InitAsDescriptorTable(1, &srvTable3);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(7, slotRootParameter,
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

	// g buffer signature
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable0;
		srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, GBufferCount, 0);

		CD3DX12_DESCRIPTOR_RANGE srvTable1;
		srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, GBufferCount);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
		slotRootParameter[2].InitAsDescriptorTable(1, &srvTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
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
			IID_PPV_ARGS(gBufferRootSignature.GetAddressOf())));
	}

	// bloom signature
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable0;
		srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE srvTable1;
		srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		CD3DX12_DESCRIPTOR_RANGE srvTable2;
		srvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[4];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
		slotRootParameter[2].InitAsDescriptorTable(1, &srvTable1);
		slotRootParameter[3].InitAsDescriptorTable(1, &srvTable2);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
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
			IID_PPV_ARGS(bloomRootSignature.GetAddressOf())));
	}

	// motion blur signature
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable0;
		srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE srvTable1;
		srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
		slotRootParameter[2].InitAsDescriptorTable(1, &srvTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
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
			IID_PPV_ARGS(motionBlurRootSignature.GetAddressOf())));
	}

	// final pass signature
	{
		CD3DX12_DESCRIPTOR_RANGE srvTable0;
		srvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_DESCRIPTOR_RANGE srvTable1;
		srvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[3];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsDescriptorTable(1, &srvTable0);
		slotRootParameter[2].InitAsDescriptorTable(1, &srvTable1);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
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
			IID_PPV_ARGS(finalPassRootSignature.GetAddressOf())));
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

		CD3DX12_DESCRIPTOR_RANGE uavTable6;
		uavTable6.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 6);

		// Root parameter can be a table, root descriptor or root constants.
		CD3DX12_ROOT_PARAMETER slotRootParameter[10];
		slotRootParameter[0].InitAsConstantBufferView(0);
		slotRootParameter[1].InitAsConstantBufferView(1);
		slotRootParameter[2].InitAsConstantBufferView(2);
		slotRootParameter[3].InitAsDescriptorTable(1, &uavTable0);
		slotRootParameter[4].InitAsDescriptorTable(1, &uavTable1);
		slotRootParameter[5].InitAsDescriptorTable(1, &uavTable2);
		slotRootParameter[6].InitAsDescriptorTable(1, &uavTable3);
		slotRootParameter[7].InitAsDescriptorTable(1, &uavTable4);
		slotRootParameter[8].InitAsDescriptorTable(1, &uavTable5);
		slotRootParameter[9].InitAsDescriptorTable(1, &uavTable6);

		auto staticSamplers = GetStaticSamplers();

		// A root signature is an array of root parameters.
		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(10, slotRootParameter,
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
	{
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
}

void Game::BuildShadersAndInputLayout()
{
	D3D_SHADER_MACRO macros[] =
	{
		{"USE_DISPLACE", imguiParams.UseDisplaceMapping && imguiParams.MeshMode == MeshMode::TERRAIN ? "1" : "0"},
		{"UNIFORM_TESSELLATION", imguiParams.Uniform ? "1" : "0"},
		{"FLAT_NORMALS", imguiParams.FlatNormals ? "1" : "0"},
		{"NUM_DIR_LIGHTS", imguiParams.DirectionalLightCount == 1 ? "1" : imguiParams.DirectionalLightCount == 2 ? "2" : "3"},
		{NULL, NULL}
	};

	char str[32];
	sprintf_s(str, "%d", imguiParams.BloomKernelSize);
	D3D_SHADER_MACRO bloomMacrosH[] =
	{
		{"HORIZONTAL_BLUR", "1"},
		{"BLOOM_KERNEL_SIZE", str},
		{NULL, NULL}
	};

	D3D_SHADER_MACRO bloomMacrosV[] =
	{
		{"BLOOM_KERNEL_SIZE", str},
		{NULL, NULL}
	};

	Shaders["OpaqueVS"] = d3dUtil::CompileShader(L"DefaultVS.hlsl", macros, "main", "vs_5_1");
	Shaders["OpaquePS"] = d3dUtil::CompileShader(L"DefaultPS.hlsl", macros, "main", "ps_5_1");
	Shaders["WireframeGS"] = d3dUtil::CompileShader(L"WireframeGS.hlsl", macros, "main", "gs_5_1");
	Shaders["WireframePS"] = d3dUtil::CompileShader(L"WireframePS.hlsl", macros, "main", "ps_5_1");
	Shaders["LightPassVS"] = d3dUtil::CompileShader(L"LightPass.hlsl", macros, "VS", "vs_5_1");
	Shaders["LightPassPS"] = d3dUtil::CompileShader(L"LightPass.hlsl", macros, "PS", "ps_5_1");
	Shaders["MotionBlurVS"] = d3dUtil::CompileShader(L"MotionBlur.hlsl", macros, "VS", "vs_5_1");
	Shaders["MotionBlurPS"] = d3dUtil::CompileShader(L"MotionBlur.hlsl", macros, "PS", "ps_5_1");
	Shaders["BloomVS"] = d3dUtil::CompileShader(L"BloomShader.hlsl", macros, "VS", "vs_5_1");
	Shaders["BloomPSThreshold"] = d3dUtil::CompileShader(L"BloomShader.hlsl", macros, "PS", "ps_5_1");
	Shaders["BloomPSMainH"] = d3dUtil::CompileShader(L"BloomShader.hlsl", bloomMacrosH, "PSMain", "ps_5_1");
	Shaders["BloomPSMainV"] = d3dUtil::CompileShader(L"BloomShader.hlsl", bloomMacrosV, "PSMain", "ps_5_1");
	Shaders["RenderQuadVS"] = d3dUtil::CompileShader(L"RenderQuad.hlsl", macros, "VS", "vs_5_1");
	Shaders["RenderQuadPS"] = d3dUtil::CompileShader(L"RenderQuad.hlsl", macros, "PS", "ps_5_1");
	Shaders["TessellationUpdate"] = d3dUtil::CompileShader(L"TessellationUpdate.hlsl", macros, "main", "cs_5_1");
	Shaders["TessellationCopyDraw"] = d3dUtil::CompileShader(L"TessellationCopyDraw.hlsl", macros, "main", "cs_5_1");

	posInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	posTexInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void Game::BuildPSOs()
{
	//
	// PSO for opaque objects
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC geoOpaquePsoDesc;
	ZeroMemory(&geoOpaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	geoOpaquePsoDesc.InputLayout = { posInputLayout.data(), (UINT)posInputLayout.size() };
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
	geoOpaquePsoDesc.NumRenderTargets = GBufferCount;
	for (int i = 0; i < GBufferCount; i++)
		geoOpaquePsoDesc.RTVFormats[i] = GBufferFormats[i];
	geoOpaquePsoDesc.SampleDesc.Count = xMsaaState ? 4 : 1;
	geoOpaquePsoDesc.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	geoOpaquePsoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&geoOpaquePsoDesc, IID_PPV_ARGS(&PSOs["Opaque"])));
	PSOs["Opaque"]->SetName(L"OpaquePSO");

	//
	// PSO for shadow map pass
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC smapPsoDesc = {};
	ZeroMemory(&smapPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	smapPsoDesc.InputLayout = { posInputLayout.data(), (UINT)posInputLayout.size() };
	smapPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	smapPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // TODO: use D3D12_CULL_MODE_FRONT (tessellation algorithm will need to be modified)
	smapPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	smapPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	smapPsoDesc.SampleMask = UINT_MAX;
	smapPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	smapPsoDesc.pRootSignature = opaqueRootSignature.Get();
	smapPsoDesc.RasterizerState.DepthBias = 100000;
	smapPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	smapPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	smapPsoDesc.pRootSignature = opaqueRootSignature.Get();
	smapPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["OpaqueVS"]->GetBufferPointer()),
		Shaders["OpaqueVS"]->GetBufferSize()
	};
	smapPsoDesc.PS = {};
	smapPsoDesc.NumRenderTargets = 0;
	smapPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	smapPsoDesc.SampleDesc.Count = xMsaaState ? 4 : 1;
	smapPsoDesc.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	smapPsoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&smapPsoDesc, IID_PPV_ARGS(&PSOs["ShadowOpaque"])));
	PSOs["ShadowOpaque"]->SetName(L"ShadowOpaquePSO");

	//
	// PSO for wireframe mode
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC geoWireframePsoDesc;
	ZeroMemory(&geoWireframePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	geoWireframePsoDesc.InputLayout = { posInputLayout.data(), (UINT)posInputLayout.size() };
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
	geoWireframePsoDesc.NumRenderTargets = GBufferCount;
	for (int i = 0; i < GBufferCount; i++)
		geoWireframePsoDesc.RTVFormats[i] = GBufferFormats[i];
	geoWireframePsoDesc.SampleDesc.Count = xMsaaState ? 4 : 1;
	geoWireframePsoDesc.SampleDesc.Quality = xMsaaState ? (xMsaaQuality - 1) : 0;
	geoWireframePsoDesc.DSVFormat = DepthStencilFormat;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&geoWireframePsoDesc, IID_PPV_ARGS(&PSOs["Wireframe"])));
	PSOs["Wireframe"]->SetName(L"WireframePSO");

	//
	// PSO for deferred light pass
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC deferredLightPsoDesc;
	ZeroMemory(&deferredLightPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	deferredLightPsoDesc.InputLayout = { posTexInputLayout.data(), (UINT)posTexInputLayout.size() };
	deferredLightPsoDesc.pRootSignature = gBufferRootSignature.Get();
	deferredLightPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["LightPassVS"]->GetBufferPointer()),
		Shaders["LightPassVS"]->GetBufferSize()
	};
	deferredLightPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["LightPassPS"]->GetBufferPointer()),
		Shaders["LightPassPS"]->GetBufferSize()
	};
	deferredLightPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	deferredLightPsoDesc.RasterizerState.DepthClipEnable = false;
	deferredLightPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	deferredLightPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	deferredLightPsoDesc.DepthStencilState.DepthEnable = false;
	deferredLightPsoDesc.SampleMask = UINT_MAX;
	deferredLightPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	deferredLightPsoDesc.NumRenderTargets = 1;
	deferredLightPsoDesc.RTVFormats[0] = AccumulationBufferFormat;
	deferredLightPsoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&deferredLightPsoDesc, IID_PPV_ARGS(&PSOs["DeferredLightPass"])));
	PSOs["DeferredLightPass"]->SetName(L"DeferredLightPassPSO");

	//
	// PSO for bloom threshold
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC bloomPsoDesc = {};
	ZeroMemory(&bloomPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	bloomPsoDesc.InputLayout = { posTexInputLayout.data(), (UINT)posTexInputLayout.size() };
	bloomPsoDesc.pRootSignature = bloomRootSignature.Get();
	bloomPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["BloomVS"]->GetBufferPointer()),
		Shaders["BloomVS"]->GetBufferSize()
	};
	bloomPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["BloomPSThreshold"]->GetBufferPointer()),
		Shaders["BloomPSThreshold"]->GetBufferSize()
	};
	bloomPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	bloomPsoDesc.RasterizerState.DepthClipEnable = false;
	bloomPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	bloomPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	bloomPsoDesc.DepthStencilState.DepthEnable = false;
	bloomPsoDesc.SampleMask = UINT_MAX;
	bloomPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	bloomPsoDesc.NumRenderTargets = 1;
	bloomPsoDesc.RTVFormats[0] = AccumulationBufferFormat;
	bloomPsoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&bloomPsoDesc, IID_PPV_ARGS(&PSOs["BloomThresholdPass"])));
	PSOs["BloomThresholdPass"]->SetName(L"BloomThresholdPassPSO");

	//
	// PSO for bloom main pass horizontal
	//
	bloomPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["BloomVS"]->GetBufferPointer()),
		Shaders["BloomVS"]->GetBufferSize()
	};
	bloomPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["BloomPSMainH"]->GetBufferPointer()),
		Shaders["BloomPSMainH"]->GetBufferSize()
	};
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&bloomPsoDesc, IID_PPV_ARGS(&PSOs["BloomHPass"])));
	PSOs["BloomHPass"]->SetName(L"BloomHPassPSO");

	//
	// PSO for bloom main pass vertical
	//
	bloomPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["BloomVS"]->GetBufferPointer()),
		Shaders["BloomVS"]->GetBufferSize()
	};
	bloomPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["BloomPSMainV"]->GetBufferPointer()),
		Shaders["BloomPSMainV"]->GetBufferSize()
	};
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&bloomPsoDesc, IID_PPV_ARGS(&PSOs["BloomVPass"])));
	PSOs["BloomVPass"]->SetName(L"BloomVPassPSO");

	//
	// PSO for motion blur
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC motionBlurPsoDesc = {};
	ZeroMemory(&motionBlurPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	motionBlurPsoDesc.InputLayout = { posTexInputLayout.data(), (UINT)posTexInputLayout.size() };
	motionBlurPsoDesc.pRootSignature = motionBlurRootSignature.Get();
	motionBlurPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["MotionBlurVS"]->GetBufferPointer()),
		Shaders["MotionBlurVS"]->GetBufferSize()
	};
	motionBlurPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["MotionBlurPS"]->GetBufferPointer()),
		Shaders["MotionBlurPS"]->GetBufferSize()
	};
	motionBlurPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	motionBlurPsoDesc.RasterizerState.DepthClipEnable = false;
	motionBlurPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	motionBlurPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	motionBlurPsoDesc.DepthStencilState.DepthEnable = false;
	motionBlurPsoDesc.SampleMask = UINT_MAX;
	motionBlurPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	motionBlurPsoDesc.NumRenderTargets = 1;
	motionBlurPsoDesc.RTVFormats[0] = AccumulationBufferFormat;
	motionBlurPsoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&motionBlurPsoDesc, IID_PPV_ARGS(&PSOs["MotionBlurPass"])));
	PSOs["MotionBlurPass"]->SetName(L"MotionBlurPassPSO");

	//
	// PSO for final render quad
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC renderQuadPsoDesc = {};
	ZeroMemory(&renderQuadPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	renderQuadPsoDesc.InputLayout = { posTexInputLayout.data(), (UINT)posTexInputLayout.size() };
	renderQuadPsoDesc.pRootSignature = finalPassRootSignature.Get();
	renderQuadPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(Shaders["RenderQuadVS"]->GetBufferPointer()),
		Shaders["RenderQuadVS"]->GetBufferSize()
	};
	renderQuadPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(Shaders["RenderQuadPS"]->GetBufferPointer()),
		Shaders["RenderQuadPS"]->GetBufferSize()
	};
	renderQuadPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	renderQuadPsoDesc.RasterizerState.DepthClipEnable = false;
	renderQuadPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	renderQuadPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	renderQuadPsoDesc.DepthStencilState.DepthEnable = false;
	renderQuadPsoDesc.SampleMask = UINT_MAX;
	renderQuadPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	renderQuadPsoDesc.NumRenderTargets = 1;
	renderQuadPsoDesc.RTVFormats[0] = BackBufferFormat;
	renderQuadPsoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(Device->CreateGraphicsPipelineState(&renderQuadPsoDesc, IID_PPV_ARGS(&PSOs["RenderQuadPass"])));
	PSOs["RenderQuadPass"]->SetName(L"RenderQuadPassPSO"); // TODO: change name to HDR Tone Mapping

	//
	// PSO for compute tessellation
	//
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
		FrameResources.push_back(std::make_unique<FrameResource>(Device.Get()));
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Game::GetAccumBufferRtvDesc()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::ACCUMULATION_BUFFER + mAccumBuffRTVIdx, RTVDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Game::GetAccumBufferSrvDesc()
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::ACCUMULATION_BUFFER + ((mAccumBuffRTVIdx + 1) % 2), CBVSRVUAVDescriptorSize);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Game::GetBloomBufferRtvDesc()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(RTVHeap->GetCPUDescriptorHandleForHeapStart(), (int)RTVIndex::BLOOM_BUFFER + mBloomBuffRTVIdx, RTVDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Game::GetBloomBufferSrvDesc()
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), (int)CBVSRVUAVIndex::BLOOM_BUFFER + ((mBloomBuffRTVIdx + 1) % 2), CBVSRVUAVDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE Game::GetSrvResourceDesc(CBVSRVUAVIndex index)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(CBVSRVUAVHeap->GetGPUDescriptorHandleForHeapStart(), (int)index, CBVSRVUAVDescriptorSize);
}

double Game::GetQueryTimestamps(ID3D12Resource* queryBuffer)
{
	UINT64* pTimestamps;
	queryBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pTimestamps));

	UINT64 deltaTimeInTicks = pTimestamps[1] - pTimestamps[0];

	UINT64 gpuFrequency;

	ComputeCommandQueue->GetTimestampFrequency(&gpuFrequency);

	double timeInMilliseconds = (deltaTimeInTicks / static_cast<double>(gpuFrequency)) * 1000.0;

	queryBuffer->Unmap(0, nullptr);

	return timeInMilliseconds;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> Game::GetStaticSamplers()
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

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp, shadow
	};
}
