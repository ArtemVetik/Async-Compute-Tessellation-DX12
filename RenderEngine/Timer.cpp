#include "Timer.h"

namespace AsyncComputeTessellation
{
	Timer::Timer(HWND handle, LPCTSTR windowTitle) : m_Window(handle), m_WindowTitle(windowTitle)
	{
		m_FpsFrameCount = 0;
		m_FpsTimeElapsed = 0;
		m_DeltaTime = 0;
		m_TotalTime = 0;

		__int64 prefFreq;
		QueryPerformanceFrequency((LARGE_INTEGER*)&prefFreq);
		m_PerfCounterSeconds = 1.0 / (double)prefFreq;

		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
		m_StartTime = now;
		m_CurrentTime = now;
		m_PreviousTime = now;
	}

	float Timer::GetDeltaTime() const
	{
		return m_DeltaTime;
	}

	float Timer::GetTotalTime() const
	{
		return m_TotalTime;
	}

	void Timer::UpdateTimer()
	{
		__int64 now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
		m_CurrentTime = now;

		m_DeltaTime = max((float)((m_CurrentTime - m_PreviousTime) * m_PerfCounterSeconds), 0.0f);

		m_TotalTime = (float)((m_CurrentTime - m_StartTime) * m_PerfCounterSeconds);

		m_PreviousTime = m_CurrentTime;
	}

	bool Timer::UpdateTitleBarStats(int& fps, float& mspf)
	{
		m_FpsFrameCount++;

		float timeDiff = 0.0f;
		timeDiff = m_TotalTime - m_FpsTimeElapsed;

		if (timeDiff < 1.0f)
			return false;

		// How long did each frame take?  (Approx)
		mspf = 1000.0f / (float)m_FpsFrameCount;
		fps = m_FpsFrameCount;

		m_FpsFrameCount = 0;
		m_FpsTimeElapsed += 1.0f;

		return true;
	}
}