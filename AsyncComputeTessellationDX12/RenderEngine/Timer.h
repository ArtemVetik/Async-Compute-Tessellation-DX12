#pragma once

#include "framework.h"

namespace AsyncComputeTessellation
{
	class RENDERENGINE_API Timer
	{
	public:
		Timer(HWND handle, LPCTSTR windowTitle);

		float GetDeltaTime() const;
		float GetTotalTime() const;

		void UpdateTimer();
		bool UpdateTitleBarStats(int& fps, float& mspf);

	private:
		double m_PerfCounterSeconds;

		float m_TotalTime;
		float m_DeltaTime;

		__int64 m_StartTime;
		__int64 m_CurrentTime;
		__int64 m_PreviousTime;

		int m_FpsFrameCount;
		float m_FpsTimeElapsed;

		HWND m_Window;
		LPCTSTR m_WindowTitle;
	};
}