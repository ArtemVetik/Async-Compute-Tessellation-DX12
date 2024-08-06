#include "Bloom.h"

Bloom::Bloom(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	mDevice = device;
	mCommandList = commandList;
}

void Bloom::UploadWeightsBuffer(ID3D12Resource* weightsBuffer, int kernelSize)
{
	if (WeightsBufferUpload)
		WeightsBufferUpload.reset();

	WeightsBufferUpload = std::make_unique<UploadBuffer<float>>(mDevice, 7, false);

	std::vector<float> weights;

	float sigma = 3.0f;
	float sum = 0.0f;
	for (int i = 0; i <= kernelSize; ++i) {
		float weight = expf(-0.5f * (i * i) / (sigma * sigma));
		weights.push_back(weight);
		sum += weight;
	}

	for (int i = 0; i < weights.size(); ++i)
		weights[i] /= sum;

	for (int i = 0; i < 3; i++)
		WeightsBufferUpload->CopyData(i, weights[i]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(weightsBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	mCommandList->CopyResource(weightsBuffer, WeightsBufferUpload->Resource());
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(weightsBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON));
}
