#pragma once

#include "d3dUtil.h"
#include "UploadBuffer.h"

class Bloom
{
public:
	Bloom(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	void UploadWeightsBuffer(ID3D12Resource* weightsBuffer);

private:
	ID3D12Device* mDevice;
	ID3D12GraphicsCommandList* mCommandList;

	std::unique_ptr<UploadBuffer<float>> WeightsBufferUpload;
};

