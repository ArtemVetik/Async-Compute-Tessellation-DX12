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
	RenderType mRenderType;

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
	ComPtr<ID3D12Resource> QueryResultBuffer[2];

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
	BYTE mAccumBuffRTVIdx;
	BYTE mBloomBuffRTVIdx;

	virtual void Resize()override;
	virtual void Update(const Timer& timer)override;
	virtual void Draw(const Timer& timer)override;

	void ExecuteGraphicsCommands(bool withSignal);
	void ResetGraphicsCommands();
	void ExecuteComputeCommands(bool withSignal);
	void RecordImGuiCommands(ImguiOutput& output);
	void UpdateMainPassCB(const Timer& timer);
	void UpdateShadowTransform(const Timer& timer);

	void BuildUAVs();
	void UploadBuffers();
	void BuildSSQuad();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();

	CD3DX12_CPU_DESCRIPTOR_HANDLE GetAccumBufferRtvDesc();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetAccumBufferSrvDesc();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetBloomBufferRtvDesc();
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetBloomBufferSrvDesc();

	CD3DX12_GPU_DESCRIPTOR_HANDLE GetSrvResourceDesc(CBVSRVUAVIndex index);

	double GetQueryTimestamps(ID3D12Resource* queryBuffer);

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

