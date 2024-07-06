#pragma once
#include "DXCore.h"
#include "Camera.h"
#include "InputManager.h"
#include "FrameResource.h"
#include "Renderable.h"
#include "GeometryGenerator.h"
#include "SystemData.h"
#include "DDSTextureLoader.h"
#include "RenderItem.h"
#include "ImguiParams.h"

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

	std::vector<D3D12_INPUT_ELEMENT_DESC> geoInputLayout;
	ComPtr<ID3D12RootSignature> opaqueRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> tessellationComputeRootSignature = nullptr;
	ComPtr<ID3D12CommandSignature> tessellationCommandSignature = nullptr;

	ComPtr<ID3D12Resource> RWVertexPool = nullptr;
	ComPtr<ID3D12Resource> RWDrawList = nullptr;
	ComPtr<ID3D12Resource> RWDrawArgs = nullptr;

	ComPtr<ID3D12Resource> DrawListUploadBuffer = nullptr;

	CD3DX12_CPU_DESCRIPTOR_HANDLE VertexPoolCPUSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexPoolGPUSRV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE VertexPoolCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE VertexPoolGPUUAV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE DrawListCPUSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE DrawListGPUSRV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE DrawListCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE DrawListGPUUAV;

	CD3DX12_CPU_DESCRIPTOR_HANDLE DrawArgsCPUUAV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE DrawArgsGPUUAV;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> Geometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> PSOs;
	std::unordered_map<std::string, std::unique_ptr<Material>> Materials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;

	std::vector<std::unique_ptr<RenderItem>> AllRitems;

	ObjectConstants MainObjectCB;

	Camera* mainCamera;

	InputManager* inputManager;

	SystemData *systemData;

	ImguiParams imguiParams;

	virtual void Resize()override;
	virtual void Update(const Timer& timer)override;
	virtual void Draw(const Timer& timer)override;

	void ImGuiDraw(bool* changePso);
	void UpdateMainPassCB(const Timer& timer);

	void BuildGeometry();
	void BuildUAVs();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

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

