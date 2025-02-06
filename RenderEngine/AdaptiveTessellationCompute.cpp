#include "AdaptiveTessellationCompute.h"
#include "GeometryGenerator.h"

#include "imgui/imgui.h"
#include "../Core/Graphics/DynamicUploadBuffer.h"

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
		m_ComputeQueue(computeQueue),
		m_UI(this),
		m_Mesh(device),
		m_PingPongCounter(0),
		m_SubdCulledBuffIdx(0)
	{
		BuildPSO();
		InitBuffers();
		ResetBuffers();
		UpdateLeafMesh();
		InitTessData();
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
		DynamicUploadBuffer objectCB(m_Device, m_ComputeQueue ? QueueID::Both : QueueID::Direct);
		objectCB.LoadData(objConstants);

		TessellationComputePass::PerFrameData frameData = {};
		auto viewProj = XMMatrixMultiply(XMLoadFloat4x4(&m_Camera->GetViewMatrix()), XMLoadFloat4x4(&m_Camera->GetProjectionMatrix()));
		XMStoreFloat4x4(&frameData.ViewProj, XMMatrixTranspose(viewProj));
		frameData.CamPosition = m_Camera->GetPosition();
		frameData.PredictedCamPosition = m_Camera->GetPosition();
		frameData.DeltaTime = timer.GetDeltaTime();
		frameData.TotalTime = timer.GetTotalTime();
		auto frustrum = m_Camera->GetFrustrumPlanes(DirectX::SimpleMath::Matrix::Identity);
		for (int i = 0; i < 6; i++)
			frameData.FrustrumPlanes[i] = frustrum.Planes[i];

		DynamicUploadBuffer frameCB(m_Device, m_ComputeQueue ? QueueID::Both : QueueID::Direct);
		frameCB.LoadData(frameData);

		if (!m_Params.Freeze)
		{
			commandContext.GetCmdList()->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_SubdCulledBuffIdx == 0 ?
				m_DrawArgs0->GetD3D12Resource() : m_DrawArgs1->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

			commandList->SetPipelineState(m_PsoData->GetComputePass()->GetUpdatePSO());
			commandList->SetComputeRootSignature(m_PsoData->GetComputePass()->GetD3D12RootSignature());

			commandList->SetComputeRootDescriptorTable(0 + m_PingPongCounter, m_SubdBufferIn->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(1 - m_PingPongCounter, m_SubdBufferOut->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(2, m_SubdCulledBuffIdx == 0 ? m_SubdBufferOutCulled0->GetUAVView()->GetGpuHandle() : m_SubdBufferOutCulled1->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(3, m_Mesh.GetVertexUAVGpu());
			commandList->SetComputeRootDescriptorTable(4, m_Mesh.GetIndexUAVGpu());
			commandList->SetComputeRootDescriptorTable(5, m_SubdCounter->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootConstantBufferView(6, objectCB.GetAllocation().GPUAddress);
			commandList->SetComputeRootConstantBufferView(7, m_TessellationData->GetD3D12Resource()->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(8, frameCB.GetAllocation().GPUAddress);
			commandList->SetComputeRootDescriptorTable(9, m_SubdCulledBuffIdx == 0 ? m_DrawArgs0->GetUAVView()->GetGpuHandle() : m_DrawArgs1->GetUAVView()->GetGpuHandle());

			commandList->Dispatch(10000, 1, 1); // TODO: figure out how many threads group to run

			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(m_SubdCounter->GetD3D12Resource()));
			commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(m_SubdCulledBuffIdx == 0 ? m_SubdBufferOutCulled0->GetD3D12Resource() : m_SubdBufferOutCulled1->GetD3D12Resource()));
			commandContext.FlushResourceBarriers();

			commandList->SetPipelineState(m_PsoData->GetComputePass()->GetCopyDrawPSO());
			commandList->Dispatch(1, 1, 1);

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_SubdCulledBuffIdx == 0 ?
				m_DrawArgs0->GetD3D12Resource() : m_DrawArgs1->GetD3D12Resource(),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));
		}

		m_PingPongCounter = 1 - m_PingPongCounter;

		if (!m_ComputeQueue)
			m_SubdCulledBuffIdx = 1;

		auto& dCommandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto dCommandList = dCommandContext.GetCmdList();

		dCommandList->SetGraphicsRootSignature(m_PsoData->GetDrawRootSignature()->GetD3D12RootSignature());
		dCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		dCommandList->SetGraphicsRootDescriptorTable(0, m_Mesh.GetVertexSRVGpu());
		dCommandList->SetGraphicsRootDescriptorTable(1, m_Mesh.GetIndexSRVGpu());
		dCommandList->SetGraphicsRootDescriptorTable(2, m_SubdCulledBuffIdx == 0 ? m_SubdBufferOutCulled1->GetSRVView()->GetGpuHandle() : m_SubdBufferOutCulled0->GetSRVView()->GetGpuHandle());

		dCommandList->SetGraphicsRootConstantBufferView(4, objectCB.GetAllocation().GPUAddress);
		dCommandList->SetGraphicsRootConstantBufferView(5, m_TessellationData->GetD3D12Resource()->GetGPUVirtualAddress());
		dCommandList->SetGraphicsRootConstantBufferView(6, frameCB.GetAllocation().GPUAddress);
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

		int subdSize = 1000000; // TODO: find out what size is needed here
		UINT64 subdBufferByteSize = sizeof(XMUINT4) * subdSize;

		CREATE_UAV_BUFFER(m_SubdBufferIn, sizeof(XMUINT4), subdSize, L"SubdBufferIn", false);
		CREATE_UAV_BUFFER(m_SubdBufferOut, sizeof(XMUINT4), subdSize, L"SubdBufferOut", false);
		CREATE_UAV_BUFFER(m_SubdBufferOutCulled0, sizeof(XMUINT4), subdSize, L"SubdBufferOutCulled0", true);
		CREATE_UAV_BUFFER(m_SubdBufferOutCulled1, sizeof(XMUINT4), subdSize, L"SubdBufferOutCulled1", true);
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

		m_LeafMeshVertex = std::make_unique<VertexBufferD3D12>(m_Device, leafVertices.data(), sizeof(XMFLOAT3), leafVertices.size());
		m_LeafMeshIndex = std::make_unique<IndexBufferD3D12>(m_Device, leafIndices.data(), sizeof(uint16_t), leafIndices.size(), DXGI_FORMAT_R16_UINT);

		TessellationComputePass::IndirectCommand command = {};
		command.VertexBufferView = m_LeafMeshVertex->GetView();
		command.IndexBufferView = m_LeafMeshIndex->GetView();
		command.DrawArguments.IndexCountPerInstance = leafIndices.size();
		command.DrawArguments.InstanceCount = 0;
		command.DrawArguments.StartIndexLocation = 0;
		command.DrawArguments.BaseVertexLocation = 0;
		command.DrawArguments.StartInstanceLocation = 0;

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