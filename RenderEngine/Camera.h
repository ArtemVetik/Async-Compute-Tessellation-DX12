#pragma once

#include <DirectXColors.h>

#include "framework.h"

#include "../Core/Graphics/RenderDeviceD3D12.h"

namespace AsyncComputeTessellation
{
	using namespace DirectX;
	using namespace EduEngine;

	struct FrustrumPlanes
	{
		DirectX::XMFLOAT4 Planes[6];
	};

	class RENDERENGINE_API Camera
	{
	public:
		Camera(RenderDeviceD3D12* device, UINT width, UINT height);

		void SetProjectionMatrix(UINT newWidth, UINT newHeight);
		void SetProjectionMatrix(float* fov = nullptr, float* nearView = nullptr, float* farView = nullptr);

		void Pitch(float angle);
		void RotateY(float angle);
		void Move(XMVECTOR deltaPos);
		void Update();

		XMFLOAT4X4 GetViewMatrix() const { return m_ViewMatrix; }
		XMFLOAT4X4 GetProjectionMatrix() const { return m_ProjectionMatrix; }
		XMFLOAT3 GetPosition() const { return m_Position; }
		XMFLOAT3 GetLook() const { return m_Look; }
		XMFLOAT3 GetRight() const { return m_Right; }
		XMFLOAT3 GetUp() const { return m_Up; }
		float GetNear() const { return m_NearValue; }
		float GetFar() const { return m_FarValue; }
		float GetFovY() const { return m_FovY; }
		float GetFovX() const { return m_FovX; }

		XMMATRIX GetViewProjMatrix() const;
		FrustrumPlanes GetFrustrumPlanes(XMMATRIX worldMatrix) const;

	private:
		RenderDeviceD3D12* m_Device;

		UINT m_ScreenWidth;
		UINT m_ScreenHeight;

		float m_FovY = 55.0f * (3.14f / 180.0f);
		float m_FovX;

		XMFLOAT4X4 m_ViewMatrix;
		XMFLOAT4X4 m_ProjectionMatrix;
		float m_NearValue;
		float m_FarValue;

		XMFLOAT3 m_Position = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 m_Right = { 1.0f, 0.0f, 0.0f };
		XMFLOAT3 m_Up = { 0.0f, 1.0f, 0.0f };
		XMFLOAT3 m_Look = { 0.0f, 0.0f, 1.0f };
		bool m_ViewDirty;
	};
}