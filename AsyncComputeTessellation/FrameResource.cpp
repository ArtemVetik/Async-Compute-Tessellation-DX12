#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(graphicsCommandListAllocator.GetAddressOf())));
	graphicsCommandListAllocator->SetName(L"graphicsCommandListAllocator");

	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COMPUTE,
		IID_PPV_ARGS(computeCommandListAllocator.GetAddressOf())));
	computeCommandListAllocator->SetName(L"computeCommandListAllocator");

	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, 2, true);
	TessellationCB = std::make_unique<UploadBuffer<TessellationConstants>>(device, 1, true);
	PerFrameCB = std::make_unique<UploadBuffer<PerFrameConstants>>(device, 1, true);
	LightPassCB = std::make_unique<UploadBuffer<LightPassConstants>>(device, 1, true);
	MotionBlurCB = std::make_unique<UploadBuffer<MotionBlurConstants>>(device, 1, true);
	BloomCB = std::make_unique<UploadBuffer<BloomConstants>>(device, 1, true);
}

FrameResource::~FrameResource()
{

}