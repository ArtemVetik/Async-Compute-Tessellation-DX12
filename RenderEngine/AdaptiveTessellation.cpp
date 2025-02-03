#include "AdaptiveTessellation.h"
#include "GeometryGenerator.h"

#include "imgui/imgui.h"
#include "../Core/Graphics/DynamicUploadBuffer.h"

namespace AsyncComputeTessellation
{
	AdaptiveTessellation::AdaptiveTessellation(RenderDeviceD3D12* device, SwapChain* swapChain, const Camera* camera) :
		m_Device(device),
		m_SwapChain(swapChain),
		m_Camera(camera),
		m_UI(this),
		m_Mesh(device),
		m_PingPongCounter(0),
		m_Freeze(false)
	{
		BuildPSO();
		InitBuffers();
		ResetBuffers();
		UpdateLeafMesh();
		InitTessData();
	}

	void AdaptiveTessellation::Compute(const Timer& timer)
	{
		int subdCulledBuffIdx = 0;

		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		auto commandList = commandContext.GetCmdList();

		commandList->SetPipelineState(m_ComputePass->GetUpdatePSO());
		commandList->SetComputeRootSignature(m_ComputePass->GetD3D12RootSignature());

		TessellationComputePass::ObjectData objConstants = {};

		DynamicUploadBuffer objBuffer(m_Device, QueueID::Direct);
		objBuffer.LoadData(objConstants);

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

		DynamicUploadBuffer frameBuffer(m_Device, QueueID::Direct);
		frameBuffer.LoadData(frameData);

		if (!m_Freeze)
		{
			commandList->SetComputeRootDescriptorTable(0 + m_PingPongCounter, m_SubdBufferIn->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(1 - m_PingPongCounter, m_SubdBufferOut->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(2, subdCulledBuffIdx == 0 ? m_SubdBufferOutCulled0->GetUAVView()->GetGpuHandle() : m_SubdBufferOutCulled1->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootDescriptorTable(3, m_Mesh.GetVertexUAVGpu());
			commandList->SetComputeRootDescriptorTable(4, m_Mesh.GetIndexUAVGpu());
			commandList->SetComputeRootDescriptorTable(5, m_SubdCounter->GetUAVView()->GetGpuHandle());
			commandList->SetComputeRootConstantBufferView(6, objBuffer.GetAllocation().GPUAddress);
			commandList->SetComputeRootConstantBufferView(7, m_TessellationData->GetD3D12Resource()->GetGPUVirtualAddress());
			commandList->SetComputeRootConstantBufferView(8, frameBuffer.GetAllocation().GPUAddress);
			commandList->SetComputeRootDescriptorTable(9, subdCulledBuffIdx == 0 ? m_DrawArgs0->GetUAVView()->GetGpuHandle() : m_DrawArgs1->GetUAVView()->GetGpuHandle());

			commandList->Dispatch(10000, 1, 1); // TODO: figure out how many threads group to run

			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_SubdBufferIn->GetD3D12Resource())); // TODO: are these lines necessary?
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_SubdBufferOut->GetD3D12Resource()));
			commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(subdCulledBuffIdx == 0 ? m_SubdBufferOutCulled0->GetD3D12Resource() : m_SubdBufferOutCulled1->GetD3D12Resource()));

			commandList->SetPipelineState(m_ComputePass->GetCopyDrawPSO());
			commandList->SetComputeRootSignature(m_ComputePass->GetD3D12RootSignature());
			commandList->Dispatch(1, 1, 1);
		}


		subdCulledBuffIdx = 1;


		D3D12_CPU_DESCRIPTOR_HANDLE gBuffViews[TessellationGBufferPass::GBufferCount];
		for (int i = 0; i < TessellationGBufferPass::GBufferCount; i++)
			gBuffViews[i] = m_GBuffer->GetGBufferRTVView(i);

		commandContext.SetRenderTargets(TessellationGBufferPass::GBufferCount, gBuffViews, false, &(m_SwapChain->DepthStencilView()));

		commandContext.GetCmdList()->ClearDepthStencilView(m_SwapChain->DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		for (int i = 0; i < TessellationGBufferPass::GBufferCount; i++)
			commandContext.GetCmdList()->ClearRenderTargetView(m_GBuffer->GetGBufferRTVView(i), DirectX::Colors::Black, 0, nullptr);

		commandList->SetPipelineState(m_DrawPass->GetD3D12PipelineState());
		commandList->SetGraphicsRootSignature(m_DrawPass->GetD3D12RootSignature());
		commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		commandList->SetGraphicsRootDescriptorTable(0, m_Mesh.GetVertexSRVGpu());
		commandList->SetGraphicsRootDescriptorTable(1, m_Mesh.GetIndexSRVGpu());
		commandList->SetGraphicsRootDescriptorTable(2, subdCulledBuffIdx == 0 ? m_SubdBufferOutCulled1->GetSRVView()->GetGpuHandle() : m_SubdBufferOutCulled0->GetSRVView()->GetGpuHandle());

		commandList->SetGraphicsRootConstantBufferView(4, objBuffer.GetAllocation().GPUAddress);
		commandList->SetGraphicsRootConstantBufferView(5, m_TessellationData->GetD3D12Resource()->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(6, frameBuffer.GetAllocation().GPUAddress);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdCulledBuffIdx == 0 ?
			m_DrawArgs1->GetD3D12Resource() : m_DrawArgs0->GetD3D12Resource(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

		commandList->ExecuteIndirect(
			m_ComputePass->GetD3D12CommandSignature(),
			1,
			subdCulledBuffIdx == 0 ? m_DrawArgs1->GetD3D12Resource() : m_DrawArgs0->GetD3D12Resource(),
			0,
			nullptr,
			0);

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(subdCulledBuffIdx == 0 ?
			m_DrawArgs1->GetD3D12Resource() : m_DrawArgs0->GetD3D12Resource(),
			D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

		m_PingPongCounter = 1 - m_PingPongCounter;
	}

	void AdaptiveTessellation::RenderImGui()
	{
		m_UI.DrawUI();
	}

	void AdaptiveTessellation::BuildPSO()
	{
		D3D_SHADER_MACRO macros[] =
		{
			{"USE_DISPLACE", m_Params.UseDisplaceMapping && m_Params.MeshMode == MeshMode::TERRAIN ? "1" : "0"},
			{"UNIFORM_TESSELLATION", m_Params.Uniform ? "1" : "0"},
			{"FLAT_NORMALS", m_Params.FlatNormals ? "1" : "0"},
			{NULL, NULL}
		};

		m_GBuffer = std::make_unique<GBuffer>(TessellationGBufferPass::GBufferCount, TessellationGBufferPass::RtvFormats, DeferredLightPass::AccumBuffFormat);
		m_ComputePass = std::make_unique<TessellationComputePass>(m_Device, QueueID::Direct, macros);
		m_DrawPass = std::make_unique<TessellationGBufferPass>(m_Device, QueueID::Direct, m_Params.WireframeMode, macros);

		m_GBuffer->Resize(m_Device, m_SwapChain->GetWidth(), m_SwapChain->GetHeight());
	}

	void AdaptiveTessellation::InitBuffers()
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
			QueueID::Direct																							\
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

	void AdaptiveTessellation::ResetBuffers()
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

	void AdaptiveTessellation::UpdateLeafMesh()
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
			QueueID::Direct
		);
		m_DrawArgs0->SetName(L"DrawArgs0");
		m_DrawArgs0->CreateUAV(&uavDesc);

		m_DrawArgs1 = std::make_unique<BufferD3D12>(m_Device,
			CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT) * drawArgsCount, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			&command,
			QueueID::Direct
		);
		m_DrawArgs1->SetName(L"DrawArgs1");
		m_DrawArgs1->CreateUAV(&uavDesc);

		auto& commandContext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);

		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_DrawArgs0->GetD3D12Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		commandContext.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(m_DrawArgs1->GetD3D12Resource(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		commandContext.FlushResourceBarriers();
	}

	void AdaptiveTessellation::InitTessData()
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

		m_TessellationData = std::make_unique<BufferD3D12>(m_Device, CD3DX12_RESOURCE_DESC::Buffer(sizeof(TessellationComputePass::TessellationData)), QueueID::Direct);
		m_TessellationData->LoadData(&m_Params.CB);
	}
}