#include "Camera.h"

namespace AsyncComputeTessellation
{
	Camera::Camera(RenderDeviceD3D12* device, UINT width, UINT height) :
		m_Device(device),
		m_ViewDirty(true)
	{
		m_NearValue = 5.0f;
		m_FarValue = 1000.0f;

		XMVECTOR pos = XMVectorSet(0.0f, 20.0f, -150.0f, 0.0f);
		XMVECTOR dir = XMVectorSet(0, 0, 1, 0);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);

		XMMATRIX V = XMMatrixLookToLH(
			pos,
			dir,
			up);
		XMStoreFloat4x4(&m_PrevViewMatrix, (V));
		XMStoreFloat4x4(&m_ViewMatrix, (V));

		SetProjectionMatrix(width, height);

		XMStoreFloat3(&m_Look, dir);
		XMStoreFloat3(&m_Up, up);
		XMStoreFloat3(&m_Right, XMVector3Cross(up, dir));
		XMStoreFloat3(&m_Position, pos);

		Update();
	}

	XMMATRIX Camera::GetViewProjMatrix() const
	{
		return DirectX::XMLoadFloat4x4(&m_ViewMatrix) * DirectX::XMLoadFloat4x4(&m_ProjectionMatrix);
	}

	FrustrumPlanes Camera::GetFrustrumPlanes(XMMATRIX worldMatrix) const
	{
		XMMATRIX view = XMLoadFloat4x4(&m_ViewMatrix);
		XMMATRIX projection = XMLoadFloat4x4(&m_ProjectionMatrix);
		XMMATRIX mvp = XMMatrixMultiply(XMMatrixMultiply(worldMatrix, view), projection);

		FrustrumPlanes planes = {};

		for (int i = 0; i < 3; ++i)
		{
			for (int j = 0; j < 2; ++j) {
				planes.Planes[i * 2 + j].x =
					mvp.r[0].m128_f32[3] + (j == 0 ? mvp.r[0].m128_f32[i] : -mvp.r[0].m128_f32[i]);
				planes.Planes[i * 2 + j].y =
					mvp.r[1].m128_f32[3] + (j == 0 ? mvp.r[1].m128_f32[i] : -mvp.r[1].m128_f32[i]);
				planes.Planes[i * 2 + j].z =
					mvp.r[2].m128_f32[3] + (j == 0 ? mvp.r[2].m128_f32[i] : -mvp.r[2].m128_f32[i]);
				planes.Planes[i * 2 + j].w =
					mvp.r[3].m128_f32[3] + (j == 0 ? mvp.r[3].m128_f32[i] : -mvp.r[3].m128_f32[i]);
			}
		}

		for (int i = 0; i < 6; i++)
		{
			float length = sqrtf(planes.Planes[i].x * planes.Planes[i].x
				+ planes.Planes[i].y * planes.Planes[i].y +
				planes.Planes[i].z * planes.Planes[i].z);
			planes.Planes[i].x /= length;
			planes.Planes[i].y /= length;
			planes.Planes[i].z /= length;
			planes.Planes[i].w /= length;
		}

		return planes;
	}

	void Camera::SetProjectionMatrix(UINT newWidth, UINT newHeight)
	{
		m_ScreenWidth = newWidth;
		m_ScreenHeight = newHeight;

		SetProjectionMatrix();
	}

	void Camera::SetProjectionMatrix(float* fov, float* nearView, float* farView)
	{
		if (fov) m_FovY = *fov;
		if (nearView) m_NearValue = *nearView;
		if (farView) m_FarValue = *farView;

		m_FovY = std::max(m_FovY, FLT_MIN);
		m_NearValue = std::max(m_NearValue, FLT_MIN);
		m_FarValue = std::max(m_FarValue, m_NearValue + 0.1f);

		auto aspectRatio = ((float)m_ScreenWidth) / ((float)m_ScreenHeight);
		if (aspectRatio == 0)
			aspectRatio = FLT_MAX;

		auto nearWindowHeight = 2.0f * m_NearValue * tanf(0.5f * m_FovY);
		float halfWidth = 0.5f * aspectRatio * nearWindowHeight;
		m_FovX = 2.0f * atan(halfWidth / m_NearValue);

		XMMATRIX P = XMMatrixPerspectiveFovLH(
			m_FovY,
			aspectRatio,
			m_NearValue,
			m_FarValue
		);
		XMStoreFloat4x4(&m_ProjectionMatrix, (P));
	}

	void Camera::Pitch(float angle)
	{
		XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&m_Right), angle);

		XMStoreFloat3(&m_Up, XMVector3TransformNormal(XMLoadFloat3(&m_Up), R));
		XMStoreFloat3(&m_Look, XMVector3TransformNormal(XMLoadFloat3(&m_Look), R));

		m_ViewDirty = true;
	}

	void Camera::RotateY(float angle)
	{
		XMMATRIX R = XMMatrixRotationY(angle);

		XMStoreFloat3(&m_Right, XMVector3TransformNormal(XMLoadFloat3(&m_Right), R));
		XMStoreFloat3(&m_Up, XMVector3TransformNormal(XMLoadFloat3(&m_Up), R));
		XMStoreFloat3(&m_Look, XMVector3TransformNormal(XMLoadFloat3(&m_Look), R));

		m_ViewDirty = true;
	}

	void Camera::Move(XMVECTOR deltaPos)
	{
		XMVECTOR pos = XMLoadFloat3(&m_Position);
		pos += deltaPos;
		XMStoreFloat3(&m_Position, pos);
		m_ViewDirty = true;
	}

	void Camera::Update()
	{
		m_PrevViewMatrix = m_ViewMatrix;

		if (m_ViewDirty)
		{
			XMVECTOR R = XMLoadFloat3(&m_Right);
			XMVECTOR U = XMLoadFloat3(&m_Up);
			XMVECTOR L = XMLoadFloat3(&m_Look);
			XMVECTOR P = XMLoadFloat3(&m_Position);

			L = XMVector3Normalize(L);
			U = XMVector3Normalize(XMVector3Cross(L, R));

			R = XMVector3Cross(U, L);

			float x = -XMVectorGetX(XMVector3Dot(P, R));
			float y = -XMVectorGetX(XMVector3Dot(P, U));
			float z = -XMVectorGetX(XMVector3Dot(P, L));

			XMStoreFloat3(&m_Right, R);
			XMStoreFloat3(&m_Up, U);
			XMStoreFloat3(&m_Look, L);

			m_ViewMatrix(0, 0) = m_Right.x;
			m_ViewMatrix(1, 0) = m_Right.y;
			m_ViewMatrix(2, 0) = m_Right.z;
			m_ViewMatrix(3, 0) = x;

			m_ViewMatrix(0, 1) = m_Up.x;
			m_ViewMatrix(1, 1) = m_Up.y;
			m_ViewMatrix(2, 1) = m_Up.z;
			m_ViewMatrix(3, 1) = y;

			m_ViewMatrix(0, 2) = m_Look.x;
			m_ViewMatrix(1, 2) = m_Look.y;
			m_ViewMatrix(2, 2) = m_Look.z;
			m_ViewMatrix(3, 2) = z;

			m_ViewMatrix(0, 3) = 0.0f;
			m_ViewMatrix(1, 3) = 0.0f;
			m_ViewMatrix(2, 3) = 0.0f;
			m_ViewMatrix(3, 3) = 1.0f;

			m_ViewDirty = false;
		}
	}
}