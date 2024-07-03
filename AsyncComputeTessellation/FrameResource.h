#pragma once
#include "d3dUtil.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "Vertex.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 Projection = MathHelper::Identity4x4();
	float AspectRatio = 0.0f;
};

struct TimeConstants
{
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;
};

struct ParticleConstants
{
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	DirectX::XMFLOAT3 velocity;
	float LifeTime = 0.0f;
	DirectX::XMFLOAT3 acceleration;
	float pad;
	int EmitCount = 0;
	int MaxParticles = 0;
	int GridSize = 0;
};

// stores the resources needed for the CPU to build the command lists for a frame 
struct FrameResource
{
public:

	FrameResource(ID3D12Device* device, UINT objectCount, UINT timeCount, UINT particleCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	// we cannot reset the allocator until the GPU is done processing the commands so each frame needs their own allocator
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandListAllocator;

	// we cannot update a cbuffer until the GPU is done processing the commands that reference it
	//so each frame needs their own cbuffers
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<TimeConstants>> TimeCB = nullptr;
	std::unique_ptr<UploadBuffer<ParticleConstants>> ParticleCB = nullptr;

	// fence value to mark commands up to this fence point 
	// this lets us check if these frame resources are still in use by the GPU.
	UINT64 Fence = 0;
};