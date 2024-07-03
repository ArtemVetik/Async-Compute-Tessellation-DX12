#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include "Camera.h"

struct Particle
{
	DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT3 Position;
	float Age;
	DirectX::XMFLOAT3 Velocity;
	float Size;
	float Alive;
	DirectX::XMFLOAT3 Padding;
};

struct ParticleSort
{
	unsigned int index;
};

class Emitter
{
public:
	Emitter(int maxParticles,
		int gridSize,
		float emissionRate,
		float lifeTime,
		DirectX::XMFLOAT3 velocity,
		DirectX::XMFLOAT3 acceleration,
		DirectX::XMFLOAT4 startColor,
		DirectX::XMFLOAT4 endColor);
	~Emitter();

	int GetEmitCount();
	int GetMaxParticles();
	int GetGridSize();
	int GetVerticesPerParticle();
	float GetLifeTime();
	float GetEmitTimeCounter();
	float GetTimeBetweenEmit();
	DirectX::XMFLOAT3 GetVelocity();
	DirectX::XMFLOAT3 GetAcceleration();
	DirectX::XMFLOAT4 GetStartColor();
	DirectX::XMFLOAT4 GetEndColor();

	void SetEmitCount(int value);
	void SetEmitTimeCounter(float value);

	void Update(float TotalTime, float deltaTime);
	
private:
	//emitter settings
	int maxParticles;
	int gridSize;
	int emitCount;
	float lifeTime;
	float emissionRate;
	float timeBetweenEmit;
	float emitTimeCounter;
	DirectX::XMFLOAT3 velocity;
	DirectX::XMFLOAT3 acceleration;
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
};

	