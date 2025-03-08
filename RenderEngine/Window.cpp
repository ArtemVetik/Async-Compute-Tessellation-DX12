#include "Window.h"

#include <cassert>

#include "RenderEngine.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace AsyncComputeTessellation
{
	LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		// Forward hwnd on because we can get messages (e.g., WM_CREATE)
		// before CreateWindow returns, and thus before mhMainWnd is valid.
		return Window::GetInstance()->MsgProc(hwnd, msg, wParam, lParam);
	}

	Window* Window::m_Instance = nullptr;

	Window::Window(HINSTANCE hInstance, int width, int height) :
		m_ApplicationInstanceHandle(hInstance),
		m_ScreenWidth(width),
		m_ScreenHeight(height)
	{
		assert(m_Instance == nullptr);
		m_Instance = this;
	}

	bool Window::Initialize()
	{
		WNDCLASS wc = { 0 };
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = GetModuleHandle(NULL);
		wc.lpszClassName = L"MainWindow";

		if (!RegisterClass(&wc))
		{
			MessageBox(0, L"RegisterClass Failed.", 0, 0);
			return false;
		}

		m_MainWindowHandle = CreateWindowEx(
			0,
			L"MainWindow",
			L"Async Compute Tessellation",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			GetClientWidth(),
			GetClientHeight(),
			NULL,
			NULL,
			GetModuleHandle(NULL),
			NULL
		);

		if (!m_MainWindowHandle)
		{
			MessageBox(0, L"CreateWindow Failed.", 0, 0);
			return false;
		}

		ShowWindow(m_MainWindowHandle, SW_SHOWNORMAL);
		UpdateWindow(m_MainWindowHandle);

		HMONITOR hMonitor = MonitorFromWindow(m_MainWindowHandle, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO mi = {};
		mi.cbSize = sizeof(mi);
		GetMonitorInfo(hMonitor, &mi);

		SetWindowLongPtr(m_MainWindowHandle, GWL_STYLE, WS_POPUP | WS_VISIBLE);

		SetWindowPos(m_MainWindowHandle, HWND_TOP,
			mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top,
			SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

		return true;
	}

	void Window::GetPosition(UINT& x, UINT& y, UINT& w, UINT& h)
	{
		RECT rect;
		GetWindowRect(GetMainWindow(), &rect);
		x = rect.left;
		y = rect.top;
		w = rect.right - rect.left;
		h = rect.bottom - rect.top;
	}

	void Window::SetPosition(UINT x, UINT y, UINT w, UINT h)
	{
		SetWindowPos(GetMainWindow(), NULL, x, y, w, h, SWP_NOZORDER);
	}

	LRESULT Window::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

		switch (msg)
		{
		case WM_SIZE:
			m_ScreenWidth = LOWORD(lParam);
			m_ScreenHeight = HIWORD(lParam);

			if (wParam == SIZE_MINIMIZED)
			{
				m_ApplicationPaused = true;
				m_ApplicationMinimized = true;
				m_ApplicationMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				m_ApplicationPaused = false;
				m_ApplicationMinimized = false;
				m_ApplicationMaximized = true;
				OnResize(m_ScreenWidth, m_ScreenHeight);
			}
			else if (wParam == SIZE_RESTORED)
			{
				// Restoring from minimized state?
				if (m_ApplicationMinimized)
				{
					m_ApplicationPaused = false;
					m_ApplicationMinimized = false;
					OnResize(m_ScreenWidth, m_ScreenHeight);
				}
				// Restoring from maximized state?
				else if (m_ApplicationMaximized)
				{
					m_ApplicationPaused = false;
					m_ApplicationMaximized = false;
					OnResize(m_ScreenWidth, m_ScreenHeight);
				}
				else if (m_ApplicationResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize(m_ScreenWidth, m_ScreenHeight);
				}
			}
			return 0;

		// WM_DESTROY is sent when the window is being destroyed.
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		/*case WM_GETMINMAXINFO:
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
			return 0;*/
		}

		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	void Window::OnResize(UINT w, UINT h)
	{
		if (RenderEngine::GetInstance())
			RenderEngine::GetInstance()->PendingResize(w, h);
	}
}