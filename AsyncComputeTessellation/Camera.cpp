#include "Camera.h"

Camera::Camera(unsigned int width, unsigned int height)
{
	InputManager::getInstance()->addMouseHandler(this);

	ResetCamera();

	nearValue = 5.0f;
	farValue = 1000.0f;

	XMVECTOR pos = XMVectorSet(0.0f, 20.0f, -150.0f, 0.0f);
	XMVECTOR dir = XMVectorSet(0, 0, 1, 0);
	XMVECTOR up = XMVectorSet(0, 1, 0, 0);

	XMMATRIX V = XMMatrixLookToLH(
		pos,
		dir,
		up);
	XMStoreFloat4x4(&prevViewMatrix, (V));
	XMStoreFloat4x4(&viewMatrix, (V));

	SetProjectionMatrix(width, height);

	XMStoreFloat3(&mPosition, pos);
	XMStoreFloat3(&mLook, dir);
	XMStoreFloat3(&mUp, up);
	XMStoreFloat3(&mRight, XMVector3Cross(up, dir));

	for (int i = 0; i < PredictionBufferSize; i++)
	{
		mPositions[i] = mPosition;
		mVelocity[i] = { 0.0f, 0.0f, 0.0f };
	}

	mPredictedPos = mPosition;
	mCurrentPredictionIndex = 0;
}

Camera::~Camera()
{
}

XMFLOAT4X4 Camera::GetPrevViewMatrix()
{
	return prevViewMatrix;
}

XMFLOAT4X4 Camera::GetViewMatrix()
{
	return viewMatrix;
}

XMFLOAT4X4 Camera::GetProjectionMatrix()
{
	return projectionMatrix;
}

float Camera::GetNear() const
{
	return nearValue;
}

float Camera::GetFar() const
{
	return farValue;
}

float Camera::GetFov() const
{
	return fov;
}

DirectX::XMFLOAT3 Camera::GetPosition() const
{
	return mPosition;
}

DirectX::XMFLOAT3 Camera::GetPredictedPosition() const
{
	return mPredictedPos;
}

FrustrumPlanes Camera::GetFrustrumPlanes(XMMATRIX worldMatrix) const
{
	XMMATRIX view = XMLoadFloat4x4(&viewMatrix);
	XMMATRIX projection = XMLoadFloat4x4(&projectionMatrix);
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

FrustrumPlanes Camera::GetPredictedFrustrumPlanes(XMMATRIX worldMatrix) const
{
	// TODO: remove code duplication
	XMFLOAT4X4 predictedViewMatrix;
	{
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPredictedPos);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		predictedViewMatrix(0, 0) = mRight.x;
		predictedViewMatrix(1, 0) = mRight.y;
		predictedViewMatrix(2, 0) = mRight.z;
		predictedViewMatrix(3, 0) = x;

		predictedViewMatrix(0, 1) = mUp.x;
		predictedViewMatrix(1, 1) = mUp.y;
		predictedViewMatrix(2, 1) = mUp.z;
		predictedViewMatrix(3, 1) = y;

		predictedViewMatrix(0, 2) = mLook.x;
		predictedViewMatrix(1, 2) = mLook.y;
		predictedViewMatrix(2, 2) = mLook.z;
		predictedViewMatrix(3, 2) = z;

		predictedViewMatrix(0, 3) = 0.0f;
		predictedViewMatrix(1, 3) = 0.0f;
		predictedViewMatrix(2, 3) = 0.0f;
		predictedViewMatrix(3, 3) = 1.0f;
	}

	XMMATRIX view = XMLoadFloat4x4(&predictedViewMatrix);
	XMMATRIX projection = XMLoadFloat4x4(&projectionMatrix);
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

void Camera::Pitch(float angle)
{
	// Rotate up and look vector about the right vector.

	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::RotateY(float angle)
{
	// Rotate the basis vectors about the world y-axis.

	XMMATRIX R = XMMatrixRotationY(angle);

	XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
	XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
	XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

	mViewDirty = true;
}

void Camera::SetProjectionMatrix(unsigned int newWidth, unsigned int newHeight)
{
	XMMATRIX P = XMMatrixPerspectiveFovLH(fov * (3.14f / 180.0f), (float)newWidth / (float)newHeight, nearValue, farValue);
	XMStoreFloat4x4(&projectionMatrix, (P));
}

void Camera::Update(const Timer& timer)
{
	XMVECTOR direction = XMLoadFloat3(&mLook);
	XMVECTOR lrVector = XMLoadFloat3(&mRight);
	XMVECTOR upVector = XMLoadFloat3(&mUp);
	XMVECTOR pos = XMLoadFloat3(&mPosition);

	float moveRate = 0.05f;

	if (InputManager::getInstance()->isKeyPressed('W') || InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_Y))
	{
		pos += (direction * moveRate);
		mViewDirty = true;
	}

	if (InputManager::getInstance()->isKeyPressed('S') || InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_A))
	{
		pos += (-direction * moveRate);
		mViewDirty = true;
	}

	if (InputManager::getInstance()->isKeyPressed('A') || InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_X))
	{
		pos += (-lrVector * moveRate);
		mViewDirty = true;
	}

	if (InputManager::getInstance()->isKeyPressed('D') || InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_B))
	{
		pos += (+lrVector * moveRate);
		mViewDirty = true;
	}

	if (InputManager::getInstance()->isKeyPressed('E') || InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_DPAD_UP))
	{
		pos += (upVector * moveRate);
		mViewDirty = true;
	}

	if (InputManager::getInstance()->isKeyPressed('Q') || InputManager::getInstance()->isControllerButtonPressed(XINPUT_GAMEPAD_DPAD_DOWN))
	{
		pos += (-upVector * moveRate);
		mViewDirty = true;
	}

	XMStoreFloat3(&mPosition, pos);

	prevViewMatrix = viewMatrix;
	if (mViewDirty)
	{
		XMVECTOR R = XMLoadFloat3(&mRight);
		XMVECTOR U = XMLoadFloat3(&mUp);
		XMVECTOR L = XMLoadFloat3(&mLook);
		XMVECTOR P = XMLoadFloat3(&mPosition);

		// Keep camera's axes orthogonal to each other and of unit length.
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));

		// U, L already ortho-normal, so no need to normalize cross product.
		R = XMVector3Cross(U, L);

		// Fill in the view matrix entries.
		float x = -XMVectorGetX(XMVector3Dot(P, R));
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&mRight, R);
		XMStoreFloat3(&mUp, U);
		XMStoreFloat3(&mLook, L);

		viewMatrix(0, 0) = mRight.x;
		viewMatrix(1, 0) = mRight.y;
		viewMatrix(2, 0) = mRight.z;
		viewMatrix(3, 0) = x;

		viewMatrix(0, 1) = mUp.x;
		viewMatrix(1, 1) = mUp.y;
		viewMatrix(2, 1) = mUp.z;
		viewMatrix(3, 1) = y;

		viewMatrix(0, 2) = mLook.x;
		viewMatrix(1, 2) = mLook.y;
		viewMatrix(2, 2) = mLook.z;
		viewMatrix(3, 2) = z;

		viewMatrix(0, 3) = 0.0f;
		viewMatrix(1, 3) = 0.0f;
		viewMatrix(2, 3) = 0.0f;
		viewMatrix(3, 3) = 1.0f;

		mViewDirty = false;
	}

	mCurrentPredictionIndex = (mCurrentPredictionIndex + 1) % PredictionBufferSize;
	mPositions[mCurrentPredictionIndex] = mPosition;
	mVelocity[mCurrentPredictionIndex] = CalcCurrentVelocity(timer.GetDeltaTime());
	
	auto acceleration = CalcCurrentAcceleration(timer.GetDeltaTime());

	mPredictedPos.x = mPositions[mCurrentPredictionIndex].x + mVelocity[mCurrentPredictionIndex].x * timer.GetDeltaTime() +
		0.5f * acceleration.x * timer.GetDeltaTime() * timer.GetDeltaTime();
	mPredictedPos.y = mPositions[mCurrentPredictionIndex].y + mVelocity[mCurrentPredictionIndex].y * timer.GetDeltaTime() +
		0.5f * acceleration.y * timer.GetDeltaTime() * timer.GetDeltaTime();
	mPredictedPos.z = mPositions[mCurrentPredictionIndex].z + mVelocity[mCurrentPredictionIndex].z * timer.GetDeltaTime() +
		0.5f * acceleration.z * timer.GetDeltaTime() * timer.GetDeltaTime();

}

void Camera::ResetCamera()
{
	XMVECTOR pos = XMVectorSet(0.0f, -10.0f, -150.0f, 0.0f);
	XMStoreFloat3(&mPosition, pos);
}

void Camera::CreateMatrices()
{

}

DirectX::XMFLOAT3 Camera::CalcCurrentVelocity(float deltaTime)
{
	auto currentPos = mPositions[mCurrentPredictionIndex];
	auto prevPosIdx = mCurrentPredictionIndex == 0 ? PredictionBufferSize - 1 : mCurrentPredictionIndex - 1;
	auto prevPos = mPositions[prevPosIdx];

	return DirectX::XMFLOAT3(
		(currentPos.x - prevPos.x) / deltaTime,
		(currentPos.y - prevPos.y) / deltaTime,
		(currentPos.z - prevPos.z) / deltaTime
	);
}

DirectX::XMFLOAT3 Camera::CalcCurrentAcceleration(float deltaTime)
{
	auto currentVelocity = mVelocity[mCurrentPredictionIndex];
	auto prevVelocityIdx = mCurrentPredictionIndex == 0 ? PredictionBufferSize - 1 : mCurrentPredictionIndex - 1;
	auto prevVelocity = mVelocity[prevVelocityIdx];

	return DirectX::XMFLOAT3(
		(currentVelocity.x - prevVelocity.x) / deltaTime,
		(currentVelocity.y - prevVelocity.y) / deltaTime,
		(currentVelocity.z - prevVelocity.z) / deltaTime
	);
}

void Camera::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		Pitch(dy);
		RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void Camera::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void Camera::OnMouseUp(WPARAM btnState, int x, int y)
{
}
