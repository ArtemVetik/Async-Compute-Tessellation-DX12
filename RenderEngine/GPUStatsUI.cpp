#include "GPUStatsUI.h"

#include <xutility>
#include <string>

#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	GPUStatsUI::GPUStatsUI(RenderDeviceD3D12* device) :
		m_Device(device)
	{
		m_RenderReadBackBuffer = std::make_unique<ReadBackBufferD3D12>(m_Device, 2, QueueID::Direct);
		m_ComputeReadBackBuffer = std::make_unique<ReadBackBufferD3D12>(m_Device, 2, QueueID::Direct);

		m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetD3D12CommandQueue()->GetTimestampFrequency(&m_DirectFrequency);
		m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE).GetD3D12CommandQueue()->GetTimestampFrequency(&m_ComputeFrequency);
	}

	void GPUStatsUI::Update(bool asyncCompute)
	{
		UINT64 computeStart, computeEnd;
		UINT64 renderStart, renderEnd;
		m_ComputeReadBackBuffer->ReadData(0, computeStart);
		m_ComputeReadBackBuffer->ReadData(1, computeEnd);
		m_RenderReadBackBuffer->ReadData(0, renderStart);
		m_RenderReadBackBuffer->ReadData(1, renderEnd);

		UINT64 computeDelta = computeEnd - computeStart;
		UINT64 totalDelta = renderEnd - renderStart;

		m_CurrentComputeTime = (computeDelta / static_cast<double>(asyncCompute ? m_ComputeFrequency : m_DirectFrequency)) * 1000.0;
		m_CurrentTotalTime = (totalDelta / static_cast<double>(m_DirectFrequency)) * 1000.0;
	}

	void GPUStatsUI::RenderImGui()
	{
		if (m_ShowStats)
		{
			if (ImGui::Button("Show Stats [X]"))
				m_ShowStats = false;
		}
		else
		{
			if (ImGui::Button("Show Stats"))
				m_ShowStats = true;
		}

		if (m_ShowStats)
		{
			ImGui::Begin("Stats", &m_ShowStats);

			if (m_PlotRefreshTime == 0)
				m_PlotRefreshTime = ImGui::GetTime();

			while (m_PlotRefreshTime < ImGui::GetTime())
			{
				m_ComputeTime[m_StatsOffset] = m_CurrentComputeTime;
				m_TotalTime[m_StatsOffset] = m_CurrentTotalTime;

				m_StatsOffset = (m_StatsOffset + 1) % PlotDataCount;
				m_PlotRefreshTime += 1.0f / 30.0f;
			}

			auto computeMax = *std::max_element(m_ComputeTime, m_ComputeTime + PlotDataCount);
			ImGui::PlotLines("GPU compute dT", m_ComputeTime,
				PlotDataCount, m_StatsOffset,
				std::to_string(m_CurrentComputeTime).c_str(),
				0.0f, computeMax, ImVec2(0, PlotDataCount));

			auto totalMax = *std::max_element(m_TotalTime, m_TotalTime + PlotDataCount);
			ImGui::PlotLines("GPU render dT", m_TotalTime,
				PlotDataCount, m_StatsOffset,
				std::to_string(m_CurrentTotalTime).c_str(),
				0.0f, computeMax, ImVec2(0, PlotDataCount));

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}
	}

	void GPUStatsUI::MarkRenderStart()
	{
		auto& commandConext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 2);
	}

	void GPUStatsUI::MarkRenderEnd()
	{
		auto& commandConext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 3);
		m_Device->GetQueryHeap().ResolveQueryData(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 2, 2, m_RenderReadBackBuffer.get(), 0);
	}

	void GPUStatsUI::MarkComputeStart(bool computeQueue)
	{
		auto& commandConext = m_Device->GetCommandContext( computeQueue ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 0);
	}

	void GPUStatsUI::MarkComputeEnd(bool computeQueue)
	{
		auto& commandConext = m_Device->GetCommandContext(computeQueue ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 1);
		m_Device->GetQueryHeap().ResolveQueryData(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_ComputeReadBackBuffer.get(), 0);
	}
}