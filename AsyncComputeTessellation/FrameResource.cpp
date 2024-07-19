#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT objectCount, UINT timeCount, UINT particleCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(commandListAllocator.GetAddressOf())));

	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objectCount, true);
	TessellationCB = std::make_unique<UploadBuffer<TessellationConstants>>(device, 1, true);
	PerFrameCB = std::make_unique<UploadBuffer<PerFrameConstants>>(device, timeCount, true);
}

FrameResource::~FrameResource()
{

}