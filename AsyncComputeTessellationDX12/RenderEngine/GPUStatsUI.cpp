#include "GPUStatsUI.h"

#include <xutility>
#include <string>

#include "imgui/imgui.h"

namespace AsyncComputeTessellation
{
	GPUStatsUI::GPUStatsUI(RenderDeviceD3D12* device, Camera* camera) :
		m_Device(device),
		m_Camera(camera),
		m_ExportFileName("gpu_time.txt")
	{
		for (size_t i = 0; i < RenderStatsCount; i++)
			m_RenderReadBackBuffers[i] = std::make_unique<ReadBackBufferD3D12>(m_Device, 2, QueueID::Direct);

		m_ComputeReadBackBuffer = std::make_unique<ReadBackBufferD3D12>(m_Device, 2, QueueID::Direct);

		m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT).GetD3D12CommandQueue()->GetTimestampFrequency(&m_DirectFrequency);
		m_Device->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE).GetD3D12CommandQueue()->GetTimestampFrequency(&m_ComputeFrequency);

		ResetStats();
	}

	void GPUStatsUI::Update(bool asyncCompute)
	{
		UINT64 computeStart, computeEnd;
		m_ComputeReadBackBuffer->ReadData(0, computeStart);
		m_ComputeReadBackBuffer->ReadData(1, computeEnd);

		UINT64 computeDelta = computeEnd - computeStart;
		m_CurrentComputeTime = (computeDelta / static_cast<double>(asyncCompute ? m_ComputeFrequency : m_DirectFrequency)) * 1000.0;

		UINT64 renderStart, renderEnd;
		for (size_t i = 0; i < RenderStatsCount; i++)
		{
			m_RenderReadBackBuffers[i]->ReadData(0, renderStart);
			m_RenderReadBackBuffers[i]->ReadData(1, renderEnd);

			UINT64 renderDelta = renderEnd - renderStart;
			m_CurrentRenderTime[i] = (renderDelta / static_cast<double>(m_DirectFrequency)) * 1000.0;
		}
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
			static int prevOffset = -1;
			static bool rotateCamera = false;
			static int maxRecords = 200;

			ImGui::Begin("Stats", &m_ShowStats);

			ImGui::SeparatorText("Record");

			ImGui::Checkbox("Rotate Camera", &rotateCamera);
			ImGui::InputInt("Max Records", &maxRecords);

			ImGui::BeginDisabled(m_Record);

			if (ImGui::Button("Start Record"))
			{
				prevOffset = -1;
				m_ComputeTimeExport.clear();
				for (size_t i = 0; i < RenderStatsCount; i++)
					m_GpuTimeExport[i].clear();

				m_Camera->SetRotateAroundMode(rotateCamera);
				m_Record = true;
			}

			ImGui::EndDisabled();

			ImGui::SameLine();

			ImGui::BeginDisabled(!m_Record);

			if (ImGui::Button("Stop Record") || m_ComputeTimeExport.size() >= maxRecords)
			{
				m_Camera->SetRotateAroundMode(false);
				m_Record = false;
				ExportDataToFile();

				m_ComputeTimeExport.clear();
				for (size_t i = 0; i < RenderStatsCount; i++)
					m_GpuTimeExport[i].clear();
			}

			ImGui::EndDisabled();

			ImGui::SameLine();

			ImGui::Text("Num frames: %d", m_ComputeTimeExport.size());

			ImGui::SeparatorText("");

			ImGui::SliderInt("Refresh Rate", &m_RefreshRate, 1, 60);

			if (ImGui::Button("Reset"))
				ResetStats();

			ImGui::SeparatorText("");

			if (m_PlotRefreshTime == 0)
				m_PlotRefreshTime = ImGui::GetTime();

			while (m_PlotRefreshTime < ImGui::GetTime())
			{
				m_ComputeTime[m_StatsOffset] = m_CurrentComputeTime;
				for (size_t i = 0; i < RenderStatsCount; i++)
					m_RenderTime[i][m_StatsOffset] = m_CurrentRenderTime[i];

				m_StatsOffset = (m_StatsOffset + 1) % PlotDataCount;
				m_PlotRefreshTime += 1.0f / m_RefreshRate;
			}

			int currOffset = m_StatsOffset > 0 ? m_StatsOffset - 1 : PlotDataCount - 1;

			auto computeMax = *std::max_element(m_ComputeTime, m_ComputeTime + PlotDataCount);
			ImGui::PlotLines("GPU compute dT", m_ComputeTime,
				PlotDataCount, m_StatsOffset,
				std::to_string(m_ComputeTime[currOffset]).c_str(),
				0.0f, computeMax, ImVec2(0, PlotDataCount));

			if (m_Record && prevOffset != currOffset) {
				m_ComputeTimeExport.push_back(m_ComputeTime[currOffset]);
			}

			for (size_t i = 0; i < RenderStatsCount; i++)
			{
				auto totalMax = *std::max_element(m_RenderTime[i], m_RenderTime[i] + PlotDataCount);
				ImGui::PlotLines(ToName((RenderStatType)(2 + i * 2)), m_RenderTime[i],
					PlotDataCount, m_StatsOffset,
					std::to_string(m_RenderTime[i][currOffset]).c_str(),
					0.0f, totalMax, ImVec2(0, PlotDataCount));

				if (m_Record && prevOffset != currOffset) {
					m_GpuTimeExport[i].push_back(m_RenderTime[i][currOffset]);
				}
			}

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();

			prevOffset = currOffset;
		}
	}

	void GPUStatsUI::MarkRenderStart(RenderStatType type)
	{
		auto& commandConext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, (UINT)type);
	}

	void GPUStatsUI::MarkRenderEnd(RenderStatType type)
	{
		auto& commandConext = m_Device->GetCommandContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, (UINT)type + 1);
		m_Device->GetQueryHeap().ResolveQueryData(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, (UINT)type, 2, m_RenderReadBackBuffers[(UINT)type / 2 - 1].get(), 0);
	}

	void GPUStatsUI::MarkComputeStart(bool computeQueue)
	{
		auto& commandConext = m_Device->GetCommandContext(computeQueue ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 0);
	}

	void GPUStatsUI::MarkComputeEnd(bool computeQueue)
	{
		auto& commandConext = m_Device->GetCommandContext(computeQueue ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT);
		m_Device->GetQueryHeap().EndQuery(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 1);
		m_Device->GetQueryHeap().ResolveQueryData(commandConext, D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, m_ComputeReadBackBuffer.get(), 0);
	}

	void GPUStatsUI::SetExportFilePrefix(const char* prefix)
	{
		m_ExportFileName = std::string(prefix) + "_gpu_time.txt";
	}

	void GPUStatsUI::ResetStats()
	{
		memset(m_ComputeTime, 0, sizeof(float) * PlotDataCount);
		memset(m_RenderTime, 0, sizeof(float) * RenderStatsCount * PlotDataCount);
		m_CurrentComputeTime = 0;
		memset(m_CurrentRenderTime, 0, sizeof(float) * RenderStatsCount);
		m_StatsOffset = 0;
		m_PlotRefreshTime = 0;
	}

	const char* GPUStatsUI::ToName(RenderStatType type)
	{
		switch (type)
		{
		case GPUStatsUI::RenderStatType::Total: return "GPU Total dT";
		case GPUStatsUI::RenderStatType::ShadowMap: return "GPU Shadow Map Pass dT";
		case GPUStatsUI::RenderStatType::Draw: return "GPU Draw Pass dT";
		case GPUStatsUI::RenderStatType::Light: return "GPU Light Pass dT";
		case GPUStatsUI::RenderStatType::PostProcess: return "GPU Post Process Pass dT";
		}

		return "Error";
	}

	void GPUStatsUI::ExportDataToFile()
	{
		std::ofstream file(m_ExportFileName);
		
		if (!file.is_open())
		{
			OutputDebugStringW(L"Can't open file");
			return;
		}

		file << "Compute\t";
		for (auto &value : m_ComputeTimeExport)
			file << value << "\t";

		file << "\n";

		for (size_t i = 0; i < RenderStatsCount; i++)
		{
			file << ToName((RenderStatType)(2 + i * 2)) << "\t";

			for (auto &value : m_GpuTimeExport[i])
				file << value << "\t";

			file << "\n";
		}

		file.close();
	}
}