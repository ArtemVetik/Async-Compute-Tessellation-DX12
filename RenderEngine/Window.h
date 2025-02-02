#pragma once

#include <Windows.h>

#include "framework.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx12.h"

namespace AsyncComputeTessellation
{
	class RENDERENGINE_API Window
	{
	public:
		Window(HINSTANCE hInstance, int width, int height);
		bool Initialize();

		void GetPosition(UINT& x, UINT& y, UINT& w, UINT& h);
		void SetPosition(UINT x, UINT y, UINT w, UINT h);

		int GetClientWidth() const { return m_ScreenWidth; }
		int GetClientHeight() const { return m_ScreenHeight; }
		int IsPaused() const { return m_ApplicationPaused; }
		HWND GetMainWindow() const { return m_MainWindowHandle; }
		float AspectRatio() const { return static_cast<float>(m_ScreenWidth) / m_ScreenHeight; }

		LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static Window* GetInstance() { return m_Instance; }

	private:
		void OnResize(UINT w, UINT h);
	
	private:
		static Window* m_Instance;

		HINSTANCE m_ApplicationInstanceHandle = nullptr;
		HWND m_MainWindowHandle = nullptr;

		int m_ScreenWidth = 1280;
		int m_ScreenHeight = 720;
		bool m_ApplicationPaused = false;
		bool m_ApplicationMinimized = false;
		bool m_ApplicationMaximized = false;
		bool m_ApplicationResizing = false;
		bool m_ApplicationFullScreen = false;
	};
}