#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include "InputManager.h"

using namespace DirectX;

class Camera : public MyMouseEventHandler
{
public:
	Camera(unsigned int width, unsigned int height);
	~Camera();

	XMFLOAT4X4 GetViewMatrix();
	XMFLOAT4X4 GetProjectionMatrix();
	float GetNear() const;
	float GetFar() const;
	DirectX::XMFLOAT3 GetPosition() const;

	void Pitch(float angle);
	void RotateY(float angle);
	void SetProjectionMatrix(unsigned int newWidth, unsigned int newHeight);

	void Update();
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

	XMFLOAT4X4 viewMatrix;
	XMFLOAT4X4 projectionMatrix;
	float nearValue;
	float farValue;

	POINT mLastMousePos;
	bool mViewDirty = true;

	void CreateMatrices();
};

