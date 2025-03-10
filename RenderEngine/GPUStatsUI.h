#pragma once

#include "Camera.h"

#include "../Core/Graphics/BufferD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

	class GPUStatsUI
	{
	public:
		enum class RenderStatType : UINT
		{
			Total		= 2,
			ShadowMap	= 4,
			Draw		= 6,
			Light		= 8,
			PostProcess = 10
		};

		GPUStatsUI(RenderDeviceD3D12* device, Camera* camera);

		void Update(bool asyncCompute);
		void RenderImGui();

		void MarkRenderStart(RenderStatType type);
		void MarkRenderEnd(RenderStatType type);
		void MarkComputeStart(bool computeQueue);
		void MarkComputeEnd(bool computeQueue);

	private:
		void ResetStats();
		const char* ToName(RenderStatType type);
		void ExportDataToFile(const std::string& fileName);

	private:
		static constexpr int RenderStatsCount = 5;
		RenderDeviceD3D12* m_Device;
		Camera* m_Camera;

		std::unique_ptr<ReadBackBufferD3D12> m_RenderReadBackBuffers[RenderStatsCount];
		std::unique_ptr<ReadBackBufferD3D12> m_ComputeReadBackBuffer;

		UINT64 m_DirectFrequency;
		UINT64 m_ComputeFrequency;

		static constexpr int PlotDataCount = 80;

		int m_RefreshRate = 30;
		float m_PlotRefreshTime = 0.0f;
		bool m_ShowStats = false;
		int m_StatsOffset = 0;
		float m_ComputeTime[PlotDataCount];
		float m_RenderTime[RenderStatsCount][PlotDataCount];
		float m_CurrentComputeTime = 0.0f;
		float m_CurrentRenderTime[RenderStatsCount];

		bool m_Record;
		std::vector<float> m_ComputeTimeExport;
		std::vector<float> m_GpuTimeExport[RenderStatsCount];
	};
}