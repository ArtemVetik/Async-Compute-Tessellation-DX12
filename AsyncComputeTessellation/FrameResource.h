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

struct TessellationConstants
{
	DirectX::XMFLOAT4X4 MeshWorld = MathHelper::Identity4x4();
	UINT SubdivisionLevel = 0;
	UINT ScreenRes;
	float DisplaceFactor = 10.0;
	UINT WavesAnimationFlag = 0;
	float DisplaceLacunarity = 1.99;
	float DisplacePosScale = 0.02;
	float DisplaceH = 0.96;
	float LodFactor;
};

struct PerFrameConstants
{
	DirectX::XMFLOAT3 CamPosition = {};
	float DeltaTime = 0.0f;
	float TotalTime = 0.0f;
	DirectX::XMUINT3 Padding;
};

struct IndirectCommand
{
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;
	D3D12_DRAW_INDEXED_ARGUMENTS DrawArguments;
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
	std::unique_ptr<UploadBuffer<TessellationConstants>> TessellationCB = nullptr;
	std::unique_ptr<UploadBuffer<PerFrameConstants>> PerFrameCB = nullptr;

	// fence value to mark commands up to this fence point 
	// this lets us check if these frame resources are still in use by the GPU.
	UINT64 Fence = 0;
};