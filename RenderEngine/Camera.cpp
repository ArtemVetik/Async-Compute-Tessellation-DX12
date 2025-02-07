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

		for (int i = 0; i < PredictionBufferSize; i++)
		{
			m_Positions[i] = m_Position;
			m_Velocity[i] = { 0.0f, 0.0f, 0.0f };
		}

		m_PredictedPos = m_Position;
		m_CurrentPredictionIndex = 0;
	}

	XMMATRIX Camera::GetViewProjMatrix() const
	{
		return DirectX::XMLoadFloat4x4(&m_ViewMatrix) * DirectX::XMLoadFloat4x4(&m_ProjectionMatrix);
	}

	FrustrumPlanes Camera::GetFrustrumPlanes(XMMATRIX worldMatrix) const
	{
		return GetFrustrumPlanes(worldMatrix, m_ViewMatrix);
	}

	FrustrumPlanes Camera::GetPredictedFrustrumPlanes(XMMATRIX worldMatrix) const
	{
		XMFLOAT4X4 predictedViewMatrix;
		auto r = m_Right;
		auto u = m_Up;
		auto l = m_Look;
		auto p = m_PredictedPos;
		ConstructViewMatrix(predictedViewMatrix, r, u, l, p);
		
		return GetFrustrumPlanes(worldMatrix, predictedViewMatrix);
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

	void Camera::Update(const Timer& timer)
	{
		m_PrevViewMatrix = m_ViewMatrix;

		if (m_ViewDirty)
		{
			ConstructViewMatrix(m_ViewMatrix, m_Right, m_Up, m_Look, m_Position);
			m_ViewDirty = false;
		}

		m_CurrentPredictionIndex = (m_CurrentPredictionIndex + 1) % PredictionBufferSize;
		m_Positions[m_CurrentPredictionIndex] = m_Position;
		m_Velocity[m_CurrentPredictionIndex] = CalcCurrentVelocity(timer.GetDeltaTime());

		auto acceleration = CalcCurrentAcceleration(timer.GetDeltaTime());

		m_PredictedPos.x = m_Positions[m_CurrentPredictionIndex].x + m_Velocity[m_CurrentPredictionIndex].x * timer.GetDeltaTime() +
			0.5f * acceleration.x * timer.GetDeltaTime() * timer.GetDeltaTime();
		m_PredictedPos.y = m_Positions[m_CurrentPredictionIndex].y + m_Velocity[m_CurrentPredictionIndex].y * timer.GetDeltaTime() +
			0.5f * acceleration.y * timer.GetDeltaTime() * timer.GetDeltaTime();
		m_PredictedPos.z = m_Positions[m_CurrentPredictionIndex].z + m_Velocity[m_CurrentPredictionIndex].z * timer.GetDeltaTime() +
			0.5f * acceleration.z * timer.GetDeltaTime() * timer.GetDeltaTime();
	}

	void Camera::ConstructViewMatrix(XMFLOAT4X4& view, XMFLOAT3& right, XMFLOAT3& up, XMFLOAT3& look, XMFLOAT3& pos) const
	{
		XMVECTOR R = XMLoadFloat3(&right);
		XMVECTOR U = XMLoadFloat3(&up);
		XMVECTOR L = XMLoadFloat3(&look);
		XMVECTOR P = XMLoadFloat3(&pos);

		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		R = XMVector3Cross(U, L);

		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&right, R);
		XMStoreFloat3(&up, U);
		XMStoreFloat3(&look, L);

		view(0, 0) = right.x;
		view(1, 0) = right.y;
		view(2, 0) = right.z;
		view(3, 0) = x;

		view(0, 1) = up.x;
		view(1, 1) = up.y;
		view(2, 1) = up.z;
		view(3, 1) = y;

		view(0, 2) = look.x;
		view(1, 2) = look.y;
		view(2, 2) = look.z;
		view(3, 2) = z;

		view(0, 3) = 0.0f;
		view(1, 3) = 0.0f;
		view(2, 3) = 0.0f;
		view(3, 3) = 1.0f;
	}

	FrustrumPlanes Camera::GetFrustrumPlanes(XMMATRIX worldMatrix, XMFLOAT4X4 viewMatrix) const
	{
		XMMATRIX view = XMLoadFloat4x4(&viewMatrix);
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

	XMFLOAT3 Camera::CalcCurrentVelocity(float deltaTime)
	{
		auto currentPos = m_Positions[m_CurrentPredictionIndex];
		auto prevPosIdx = m_CurrentPredictionIndex == 0 ? PredictionBufferSize - 1 : m_CurrentPredictionIndex - 1;
		auto prevPos = m_Positions[prevPosIdx];

		return DirectX::XMFLOAT3(
			(currentPos.x - prevPos.x) / deltaTime,
			(currentPos.y - prevPos.y) / deltaTime,
			(currentPos.z - prevPos.z) / deltaTime
		);
	}

	XMFLOAT3 Camera::CalcCurrentAcceleration(float deltaTime)
	{
		auto currentVelocity = m_Velocity[m_CurrentPredictionIndex];
		auto prevVelocityIdx = m_CurrentPredictionIndex == 0 ? PredictionBufferSize - 1 : m_CurrentPredictionIndex - 1;
		auto prevVelocity = m_Velocity[prevVelocityIdx];

		return DirectX::XMFLOAT3(
			(currentVelocity.x - prevVelocity.x) / deltaTime,
			(currentVelocity.y - prevVelocity.y) / deltaTime,
			(currentVelocity.z - prevVelocity.z) / deltaTime
		);
	}
}