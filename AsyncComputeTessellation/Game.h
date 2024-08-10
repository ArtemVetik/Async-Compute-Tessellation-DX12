#pragma once

#define NOMINMAX

#include "DXCore.h"
#include "Camera.h"
#include "InputManager.h"
#include "FrameResource.h"
#include "Renderable.h"
#include "GeometryGenerator.h"
#include "DDSTextureLoader.h"
#include "ImguiParams.h"
#include "Bintree.h"
#include "ShadowMap.h"
#include "Bloom.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;


class Game : public DXCore
{
public:
	Game(HINSTANCE hInstance);
	Game(const Game& rhs) = delete;
	Game& operator=(const Game& rhs) = delete;
	~Game();

	virtual bool Initialize()override;

private:
	std::vector<std::unique_ptr<FrameResource>> FrameResources;
	FrameResource* currentFrameResource = nullptr;
	int currentFrameResourceIndex = 0;

	std::vector<D3D12_INPUT_ELEMENT_DESC> posInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> posTexInputLayout;
	ComPtr<ID3D12RootSignature> opaqueRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> gBufferRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> motionBlurRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> bloomRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> finalPassRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> tessellationComputeRootSignature = nullptr;
	ComPtr<ID3D12CommandSignature> tessellationCommandSignature = nullptr;

	ComPtr<ID3D12Resource> RWMeshDataVertex = nullptr;
	ComPtr<ID3D12Resource> RWMeshDataIndex = nullptr;
	ComPtr<ID3D12Resource> RWDrawArgs0 = nullptr;
	ComPtr<ID3D12Resource> RWDrawArgs1 = nullptr;
	ComPtr<ID3D12Resource> RWSubdBufferIn = nullptr;
	ComPtr<ID3D12Resource> RWSubdBufferOut = nullptr;
	ComPtr<ID3D12Resource> RWSubdBufferOutCulled0 = nullptr;
	ComPtr<ID3D12Resource> RWSubdBufferOutCulled1 = nullptr;
	ComPtr<ID3D12Resource> RWSubdCounter = nullptr;
	ComPtr<ID3D12Resource> RWBloomWeights = nullptr;

	CD3DX12_CPU_DESCRIPTOR_HANDLE MeshDataVertexCPUSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE MeshDataVertexGPUSRV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE MeshDataVertexCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE MeshDataVertexGPUUAV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE MeshDataIndexCPUSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE MeshDataIndexGPUSRV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE MeshDataIndexCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE MeshDataIndexGPUUAV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE DrawArgsCPUUAV0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE DrawArgsGPUUAV0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE DrawArgsCPUUAV1;
	CD3DX12_GPU_DESCRIPTOR_HANDLE DrawArgsGPUUAV1;
								  
	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdBufferInCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdBufferInGPUUAV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdBufferOutCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdBufferOutGPUUAV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdBufferOutCulledCPUSRV0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdBufferOutCulledGPUSRV0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdBufferOutCulledCPUUAV0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdBufferOutCulledGPUUAV0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdBufferOutCulledCPUSRV1;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdBufferOutCulledGPUSRV1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdBufferOutCulledCPUUAV1;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdBufferOutCulledGPUUAV1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE SubdCounterCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE SubdCounterGPUUAV;

	CD3DX12_GPU_DESCRIPTOR_HANDLE GBufferGPUSRV;

	CD3DX12_GPU_DESCRIPTOR_HANDLE BloomWeightsGPUSRV;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;

	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	Bintree* bintree; // TODO: make unique ptr
	Bloom* bloom; // TODO: make unique ptr

	Camera* mainCamera;

	std::unique_ptr<ShadowMap> mShadowMap;

	std::unique_ptr<MeshGeometry> ssQuadMesh;

	InputManager* inputManager;

	ImguiParams imguiParams;

	BYTE pingPongCounter;
	BYTE subdCulledBuffIdx;

	virtual void Resize()override;
	virtual void Update(const Timer& timer)override;
	virtual void Draw(const Timer& timer)override;

	void RecordComputeCommands(const Timer& timer);
	void ImGuiDraw(ImguiOutput& output);
	void UpdateMainPassCB(const Timer& timer);
	void UpdateShadowTransform(const Timer& timer);

	void BuildUAVs();
	void UploadBuffers();
	void BuildSSQuad();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

	// We pack the UAV counter into the same buffer as the commands rather than create
	// a separate 64K resource/heap for it. The counter must be aligned on 4K boundaries,
	// so we pad the command buffer (if necessary) such that the counter will be placed
	// at a valid location in the buffer.
	static inline UINT AlignForUavCounter(UINT bufferSize)
	{
		const UINT alignment = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
		return (bufferSize + (alignment - 1)) & ~(alignment - 1);
	}
};

