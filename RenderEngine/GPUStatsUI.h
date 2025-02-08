#pragma once
#include "../Core/Graphics/BufferD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace EduEngine;

	class GPUStatsUI
	{
	public:
		GPUStatsUI(RenderDeviceD3D12* device);

		void Update(bool asyncCompute);
		void RenderImGui();

		void MarkRenderStart();
		void MarkRenderEnd();
		void MarkComputeStart(bool computeQueue);
		void MarkComputeEnd(bool computeQueue);

	private:
		RenderDeviceD3D12* m_Device;

		std::unique_ptr<ReadBackBufferD3D12> m_RenderReadBackBuffer;
		std::unique_ptr<ReadBackBufferD3D12> m_ComputeReadBackBuffer;

		UINT64 m_DirectFrequency;
		UINT64 m_ComputeFrequency;

		static constexpr int PlotDataCount = 80;

		bool m_ShowStats = false;
		float m_PlotRefreshTime = 0.0f;
		int m_StatsOffset = 0;
		float m_ComputeTime[PlotDataCount];
		float m_TotalTime[PlotDataCount];
		float m_CurrentComputeTime = 0.0f;
		float m_CurrentTotalTime = 0.0f;
	};
}