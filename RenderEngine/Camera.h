#pragma once
#include <DirectXColors.h>

#include "framework.h"

#include "../Core/Graphics/RenderDeviceD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace DirectX;
	using namespace EduEngine;

	class RENDERENGINE_API Camera
	{
	public:
		Camera(RenderDeviceD3D12* device, UINT width, UINT height);

		void SetProjectionMatrix(UINT newWidth, UINT newHeight);
		void SetProjectionMatrix(float* fov = nullptr, float* nearView = nullptr, float* farView = nullptr);

		void Update(DirectX::XMFLOAT3 look, DirectX::XMFLOAT3 right, DirectX::XMFLOAT3 up, DirectX::XMFLOAT3 pos);
		void SetViewport(XMFLOAT4 viewport);
		void SetBackgroundColor(XMFLOAT4 color);

		XMFLOAT4X4 GetViewMatrix() const;
		XMFLOAT4X4 GetProjectionMatrix() const;
		XMMATRIX GetViewProjMatrix() const;
		XMFLOAT3 GetPosition() const;
		XMFLOAT4 GetViewport() const;
		XMFLOAT4 GetBackgroundColor() const;
		float GetNear() const;
		float GetFar() const;
		float GetFovY() const;
		float GetFovX() const;

	private:
		RenderDeviceD3D12* m_Device;

		UINT m_ScreenWidth;
		UINT m_ScreenHeight;

		float m_FovY = 55.0f * (3.14f / 180.0f);
		float m_FovX;

		XMFLOAT4 m_Viewport;
		XMFLOAT4 m_BackgroundColor;
		XMFLOAT4X4 m_ViewMatrix;
		XMFLOAT4X4 m_ProjectionMatrix;
		float m_NearValue;
		float m_FarValue;

		XMFLOAT3 m_Position;
	};
}