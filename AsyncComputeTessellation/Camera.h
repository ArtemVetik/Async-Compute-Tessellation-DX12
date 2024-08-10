#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include "InputManager.h"
#include "Timer.h"

using namespace DirectX;

struct FrustrumPlanes
{
	DirectX::XMFLOAT4 Planes[6];
};

class Camera : public MyMouseEventHandler
{
public:
	Camera(unsigned int width, unsigned int height);
	~Camera();

	XMFLOAT4X4 GetPrevViewMatrix();
	XMFLOAT4X4 GetViewMatrix();
	XMFLOAT4X4 GetProjectionMatrix();
	float GetNear() const;
	float GetFar() const;
	float GetFov() const;
	DirectX::XMFLOAT3 GetPosition() const;
	DirectX::XMFLOAT3 GetPredictedPosition() const;
	FrustrumPlanes GetFrustrumPlanes(XMMATRIX worldMatrix) const;
	FrustrumPlanes GetPredictedFrustrumPlanes(XMMATRIX worldMatrix) const;

	void Pitch(float angle);
	void RotateY(float angle);
	void SetProjectionMatrix(unsigned int newWidth, unsigned int newHeight);

	void Update(const Timer& timer);
	void ResetCamera();

	void OnMouseMove(WPARAM btnState, int x, int y) override;
	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;

private:
	unsigned int screenWidth;
	unsigned int screenHeight;

	DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
	DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };
	float fov = 55.0f;

	XMFLOAT4X4 prevViewMatrix;
	XMFLOAT4X4 viewMatrix;
	XMFLOAT4X4 projectionMatrix;
	float nearValue;
	float farValue;

	POINT mLastMousePos;
	bool mViewDirty = true;

	static const int PredictionBufferSize = 4;
	DirectX::XMFLOAT3 mPositions[PredictionBufferSize];
	DirectX::XMFLOAT3 mVelocity[PredictionBufferSize];
	DirectX::XMFLOAT3 mPredictedPos;
	int mCurrentPredictionIndex = 0;

	void CreateMatrices();
	DirectX::XMFLOAT3 CalcCurrentVelocity(float deltaTime);
	DirectX::XMFLOAT3 CalcCurrentAcceleration(float deltaTime);
};

