#include "AdaptiveTessellationCompute.h"
#include "GeometryGenerator.h"

#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	AdaptiveTessellationCompute::AdaptiveTessellationCompute(RenderDeviceD3D12*   device,
															 SwapChain*			  swapChain,
															 const Camera*		  camera,
															 TessellationPSOData* psoData,
															 bool				  computeQueue) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_Camera(camera),
		m_PsoData(psoData),
		m_UI(this),
		m_Mesh(device),
		m_PingPongCounter(0),
		m_SubdCulledBuffIdx(0),
		m_ObjectCB(m_Device, m_ComputeQueue ? QueueID::Both : QueueID::Direct),
		m_FrameCB(m_Device, m_ComputeQueue ? QueueID::Both : QueueID::Direct)
	{
		ForceRebuildAll(computeQueue);
	}

	void AdaptiveTessellationCompute::Compute(const Timer& timer)
	{
		if (m_ComputeQueue)
			m_SubdCulledBuffIdx = 1 - m_SubdCulledBuffIdx;
		else
			m_SubdCulledBuffIdx = 0;

		auto& commandContext = m_Device->GetCommandContext(m_ComputeQueue ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto commandList = commandContext.GetCmdList();

		TessellationComputePass::ObjectData objConstants = {};
		m_ObjectCB.LoadData(objConstants);

		TessellationComputePass::PerFrameData frameData = {};
		auto viewProj = XMMatrixMultiply(XMLoadFloat4x4(&m_Camera->GetViewMatrix()), XMLoadFloat4x4(&m_Camera->GetProjectionMatrix()));
		XMStoreFloat4x4(&frameData.ViewProj, XMMatrixTranspose(viewProj));
		frameData.CamPosition = m_Camera->GetPosition();
		frameData.PredictedCamPosition = m_Camera->GetPredictedPosition();
		frameData.DeltaTime = timer.GetDeltaTime();
		frameData.TotalTime = timer.GetTotalTime();

		auto frustrum = m_Camera->GetPredictedFrustrumPlanes(DirectX::SimpleMath::Matrix::Identity);
		for (int i = 0; i < 6; i++)
			frameData.FrustrumPlanes[i] = frustrum.Planes[i];

		m_FrameCB.LoadData(frameData);

		PIXBeginEvent(commandList, PIX_COLOR(255, 165, 0), L"ComputeTessellation");

		if (!m_Params.Freeze)
		{
			commandContext.GetCmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_SubdCulledBuffIdx == 0 ?
				m_DrawArgs0->GetD3D12Resource() : m_DrawArgs1->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			commandList->SetPipelineState(m_PsoData->GetComputePass()->GetUpdatePSO());
			commandList->SetComputeRootSignature(m_PsoData->GetComputePass()->GetD3D12RootSignature());

			UINT id = 0;

			commandList->SetComputeRootDescriptorTable(id++ + m_PingPongCounter, m_SubdBufferIn->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(id++ - m_PingPongCounter, m_SubdBufferOut->GetUAVView()->GetGpuHandle());
#ifdef USE_STANDART_TESSELLATION
			commandList->SetComputeRootDescriptorTable(id++, m_SubdCulledBuffIdx == 0 ? m_SubdBufferOutCulled0->GetUAVView()->GetGpuHandle() : m_SubdBufferOutCulled1->GetUAVView()->GetGpuHandle());
#else
			commandList->SetComputeRootDescriptorTable(id++, m_SubdBufferOutCulled->GetUAVView()->GetGpuHandle());
#endif
			commandList->SetComputeRootDescriptorTable(id++, m_Mesh.GetVertexSRVGpu());
			commandList->SetComputeRootDescriptorTable(id++, m_Mesh.GetIndexSRVGpu());
#ifdef USE_STANDART_TESSELLATION
			commandList->SetComputeRootDescriptorTable(id++, m_SubdCounter->GetUAVView()->GetGpuHandle());
#else
			commandList->SetComputeRootDescriptorTable(id++, (m_SubdCulledBuffIdx == 0 ? m_VSPrepassOutV[0] : m_VSPrepassOutV[1])->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(id++, (m_SubdCulledBuffIdx == 0 ? m_VSPrepassOutIdx[0] : m_VSPrepassOutIdx[1])->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(id++, m_SubdCounter->GetUAVView()->GetGpuHandle());
#endif
			
			commandList->SetComputeRootConstantBufferView(id++, m_ObjectCB.GetAllocation().GPUAddress);
			commandList->SetComputeRootConstantBufferView(id++, m_TessellationData->GetD3D12Resource()->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(id++, m_FrameCB.GetAllocation().GPUAddress);
			commandList->SetComputeRootDescriptorTable(id++, m_SubdCulledBuffIdx == 0 ? m_DrawArgs0->GetUAVView()->GetGpuHandle() : m_DrawArgs1->GetUAVView()->GetGpuHandle());
			
#ifndef USE_STANDART_TESSELLATION
			commandList->SetComputeRootDescriptorTable(id++, m_LeafMeshVertex->GetSRVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(id++, m_LeafMeshIndex->GetSRVView()->GetGpuHandle());
#endif

			commandList->Dispatch(m_Params.Dispatch1Count, 1, 1); // TODO: figure out how many threads group to run

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(m_SubdCounter->GetD3D12Resource()));
#ifdef USE_STANDART_TESSELLATION
			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(m_SubdCulledBuffIdx == 0 ? m_SubdBufferOutCulled0->GetD3D12Resource() : m_SubdBufferOutCulled1->GetD3D12Resource()));
#endif
			commandContext.FlushResourceBarriers();

#ifndef USE_STANDART_TESSELLATION
			commandList->SetPipelineState(m_PsoData->GetComputePass()->GetVSPrepassPSO());
			commandList->Dispatch(m_Params.Dispatch2Count, 1, 1);
#endif

			commandList->SetPipelineState(m_PsoData->GetComputePass()->GetCopyDrawPSO());
			commandList->Dispatch(1, 1, 1);

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_SubdCulledBuffIdx == 0 ?
				m_DrawArgs0->GetD3D12Resource() : m_DrawArgs1->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
		}

		PIXEndEvent(commandList);

		m_PingPongCounter = 1 - m_PingPongCounter;

		if (!m_ComputeQueue)
			m_SubdCulledBuffIdx = 1;
	}

	void AdaptiveTessellationCompute::PrepareDraw()
	{
		auto& dCommandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto dCommandList = dCommandContext.GetCmdList();

		dCommandList->SetGraphicsRootSignature(m_PsoData->GetDrawRootSignature()->GetD3D12RootSignature());
		dCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		UINT id = 0;

#ifdef USE_STANDART_TESSELLATION
		dCommandList->SetGraphicsRootDescriptorTable(id++, m_Mesh.GetVertexSRVGpu());
		dCommandList->SetGraphicsRootDescriptorTable(id++, m_Mesh.GetIndexSRVGpu());
		dCommandList->SetGraphicsRootDescriptorTable(id++, m_SubdCulledBuffIdx == 0 ? m_SubdBufferOutCulled1->GetSRVView()->GetGpuHandle() : m_SubdBufferOutCulled0->GetSRVView()->GetGpuHandle());
#endif

		id++;
		dCommandList->SetGraphicsRootConstantBufferView(id++, m_ObjectCB.GetAllocation().GPUAddress);
		dCommandList->SetGraphicsRootConstantBufferView(id++, m_TessellationData->GetD3D12Resource()->GetGPUVirtualAddress());
		dCommandList->SetGraphicsRootConstantBufferView(id++, m_FrameCB.GetAllocation().GPUAddress);
	}

	void AdaptiveTessellationCompute::ExecuteIndirect() const
	{
		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.GetCmdList()->ExecuteIndirect(
			m_PsoData->GetComputePass()->GetD3D12CommandSignature(),
			1,
			m_SubdCulledBuffIdx == 0 ? m_DrawArgs1->GetD3D12Resource() : m_DrawArgs0->GetD3D12Resource(),
			0,
			nullptr,
			0);
	}

	void AdaptiveTessellationCompute::RenderImGui()
	{
		m_UI.DrawUI();
	}

	void AdaptiveTessellationCompute::ForceRebuildAll(bool computeQueue)
	{
		m_ComputeQueue = computeQueue;

		m_ObjectCB = DynamicUploadBuffer(m_Device, m_ComputeQueue ? QueueID::Both : QueueID::Direct);
		m_FrameCB = DynamicUploadBuffer(m_Device, m_ComputeQueue ? QueueID::Both : QueueID::Direct);

		m_Params.CB.ScreenRes = std::max(m_SwapChain->GetWidth(), m_SwapChain->GetHeight());

		BuildPSO();
		InitBuffers();
		ResetBuffers();
		UpdateLeafMesh();
		InitTessData();
	}

	void AdaptiveTessellationCompute::BuildPSO()
	{
		m_PsoData->Rebuild(m_Params);
	}

	void AdaptiveTessellationCompute::InitBuffers()
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;

#define CREATE_UAV_BUFFER(buffer, byteStride, numElements, name, createSrv)											\
		uavDesc.Buffer.NumElements = numElements;																	\
		uavDesc.Buffer.StructureByteStride = byteStride;															\
		srvDesc.Buffer.NumElements = numElements;																	\
		srvDesc.Buffer.StructureByteStride = byteStride;															\
																													\
		buffer = std::make_unique<BufferD3D12>(m_Device,															\
			CD3DX12_RESOURCE_DESC::Buffer(byteStride * numElements, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),	\
			m_ComputeQueue ? QueueID::Both : QueueID::Direct														\
		);																											\
		buffer->SetName(name);																						\
		buffer->CreateUAV(&uavDesc);																				\
		if (createSrv) buffer->CreateSRV(&srvDesc);																	\

		int subdSize = 10000000; // TODO: find out what size is needed here

		CREATE_UAV_BUFFER(m_SubdBufferIn, sizeof(XMUINT4), subdSize, L"SubdBufferIn", false);
		CREATE_UAV_BUFFER(m_SubdBufferOut, sizeof(XMUINT4), subdSize, L"SubdBufferOut", false);
#ifdef USE_STANDART_TESSELLATION
		CREATE_UAV_BUFFER(m_SubdBufferOutCulled0, sizeof(XMUINT4), subdSize, L"SubdBufferOutCulled0", true);
		CREATE_UAV_BUFFER(m_SubdBufferOutCulled1, sizeof(XMUINT4), subdSize, L"SubdBufferOutCulled1", true);
#else
		CREATE_UAV_BUFFER(m_SubdBufferOutCulled, sizeof(XMUINT4), subdSize, L"SubdBufferOutCulled", false);
		subdSize *= 2;
		CREATE_UAV_BUFFER(m_VSPrepassOutV[0], sizeof(VertexOut), subdSize, L"VSPrepassOutV0", false);
		CREATE_UAV_BUFFER(m_VSPrepassOutV[1], sizeof(VertexOut), subdSize, L"VSPrepassOutV1", false);
		CREATE_UAV_BUFFER(m_VSPrepassOutIdx[0], sizeof(UINT), subdSize, L"VSPrepassOutIdx0", false);
		CREATE_UAV_BUFFER(m_VSPrepassOutIdx[1], sizeof(UINT), subdSize, L"VSPrepassOutIdx1", false);
#endif
		CREATE_UAV_BUFFER(m_SubdCounter, sizeof(UINT), 3, L"SubdCounter", false);

#undef CREATE_UAV_BUFFER
	}

	void AdaptiveTessellationCompute::ResetBuffers()
	{
		m_Mesh.Initialize(m_Params.MeshMode);

		UINT byteSize = m_Mesh.GetMeshData().Indices32.size() / 3 * sizeof(XMUINT4);
		XMUINT4* subdData = new XMUINT4[m_Mesh.GetMeshData().Indices32.size() / 3 + 1];

		for (int i = 0; i < m_Mesh.GetMeshData().Indices32.size() / 3; i++)
			subdData[i] = DirectX::XMUINT4(0, 0x1, i * 3, 1);

		m_SubdBufferIn->LoadData(subdData, &byteSize);

		for (int i = 0; i < m_Mesh.GetMeshData().Indices32.size() / 3; i++)
			subdData[i] = DirectX::XMUINT4(0, 0, 0, 0);

		m_SubdBufferOut->LoadData(subdData, &byteSize);

		delete[] subdData;

		UINT counterData[3] = { m_Mesh.GetMeshData().Indices32.size() / 3, 0, 0 };
		m_SubdCounter->LoadData(counterData);

		m_PingPongCounter = 0;
	}

	void AdaptiveTessellationCompute::UpdateLeafMesh()
	{
		auto leafVertices = m_Mesh.GetLeafVertices(m_Params.CPULodLevel);
		auto leafIndices = m_Mesh.GetLeafIndices(m_Params.CPULodLevel);

		m_LeafMeshVertex = std::make_unique<VertexBufferD3D12>(m_Device, leafVertices.data(), sizeof(XMFLOAT2), leafVertices.size());
#ifdef USE_STANDART_TESSELLATION
		m_LeafMeshIndex = std::make_unique<IndexBufferD3D12>(m_Device, leafIndices.data(), sizeof(uint16_t), leafIndices.size(), DXGI_FORMAT_R16_UINT);
#else
		// TODO: use DXGI_FORMAT_R16_UINT
		m_LeafMeshIndex = std::make_unique<IndexBufferD3D12>(m_Device, leafIndices.data(), sizeof(uint32_t), leafIndices.size(), DXGI_FORMAT_R32_UINT);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = leafVertices.size();
		srvDesc.Buffer.StructureByteStride = sizeof(XMFLOAT2);

		m_LeafMeshVertex->CreateSRV(&srvDesc);

		srvDesc.Buffer.NumElements = leafIndices.size();
		srvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
		m_LeafMeshIndex->CreateSRV(&srvDesc);

		m_Params.CB.IndicesCount = leafIndices.size();
		m_Params.CB.TrianglesCount = leafVertices.size();
		InitTessData();
#endif

#ifdef USE_STANDART_TESSELLATION
		TessellationComputePass::IndirectCommand command = {};
		command.VertexBufferView = m_LeafMeshVertex->GetView();
		command.IndexBufferView = m_LeafMeshIndex->GetView();
		command.DrawArguments.IndexCountPerInstance = leafIndices.size();
		command.DrawArguments.InstanceCount = 0;
		command.DrawArguments.StartIndexLocation = 0;
		command.DrawArguments.BaseVertexLocation = 0;
		command.DrawArguments.StartInstanceLocation = 0;
#else
		TessellationComputePass::IndirectCommand command = {};
		command.VertexBufferView.BufferLocation = m_VSPrepassOutV[0]->GetD3D12Resource()->GetGPUVirtualAddress();
		command.VertexBufferView.SizeInBytes = sizeof(VertexOut) * 20000000;
		command.VertexBufferView.StrideInBytes = sizeof(VertexOut);
		command.IndexBufferView.BufferLocation = m_VSPrepassOutIdx[0]->GetD3D12Resource()->GetGPUVirtualAddress();
		command.IndexBufferView.SizeInBytes = sizeof(UINT) * 20000000;
		command.IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
		command.DrawArguments.InstanceCount = 1;
		command.DrawArguments.StartIndexLocation = 0;
		command.DrawArguments.BaseVertexLocation = 0;
		command.DrawArguments.StartInstanceLocation = 0;
#endif

		int drawArgsCount = sizeof(TessellationComputePass::IndirectCommand) / sizeof(UINT);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.NumElements = drawArgsCount;
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

		m_DrawArgs0 = std::make_unique<BufferD3D12>(m_Device,
			CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT) * drawArgsCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			&command,
			m_ComputeQueue ? QueueID::Both : QueueID::Direct
		);
		m_DrawArgs0->SetName(L"DrawArgs0");
		m_DrawArgs0->CreateUAV(&uavDesc);

#ifndef USE_STANDART_TESSELLATION
		command.VertexBufferView.BufferLocation = m_VSPrepassOutV[1]->GetD3D12Resource()->GetGPUVirtualAddress();
		command.IndexBufferView.BufferLocation = m_VSPrepassOutIdx[1]->GetD3D12Resource()->GetGPUVirtualAddress();
#endif

		m_DrawArgs1 = std::make_unique<BufferD3D12>(m_Device,
			CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT) * drawArgsCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			&command,
			m_ComputeQueue ? QueueID::Both : QueueID::Direct
		);
		m_DrawArgs1->SetName(L"DrawArgs1");
		m_DrawArgs1->CreateUAV(&uavDesc);

		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_DrawArgs0->GetD3D12Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_DrawArgs1->GetD3D12Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
		commandContext.FlushResourceBarriers();

		// If the ComputeQueue is used, ExecuteCommandLists must be called in advance
		// to avoid simultaneous access errors to the DrawArgs buffers.
		if (m_ComputeQueue)
		{
			auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_COMPUTE);
			m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE).CloseAndExecuteCommandContext(&commandContext);
			commandContext.Reset();
		}
	}

	void AdaptiveTessellationCompute::InitTessData()
	{
		float l = 2.0f * tan(m_Camera->GetFovY() / 2.0f)
			* m_Params.TargetLength
			* (1 << m_Params.CPULodLevel)
			/ float(m_Params.CB.ScreenRes);

		const float cap = 0.43f;
		if (l > cap) {
			l = cap;
		}

		m_Params.CB.LodFactor = l / float(m_Mesh.GetMeshData().GetAvgEdgeLength()); 

		m_TessellationData = std::make_unique<BufferD3D12>(m_Device, CD3DX12_RESOURCE_DESC::Buffer(sizeof(TessellationComputePass::TessellationData)), m_ComputeQueue ? QueueID::Both : QueueID::Direct);
		m_TessellationData->LoadData(&m_Params.CB);
	}
}