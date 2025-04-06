#pragma once
#include "framework.h"
#include "CommandContext.h"

#include <d3d12.h>

namespace EduEngine
{
	class GRAPHICS_API RenderDeviceD3D12;
	class GRAPHICS_API ReadBackBufferD3D12;

	class GRAPHICS_API QueryHeap
	{
	public:
		QueryHeap(RenderDeviceD3D12* device, UINT numQueries, D3D12_QUERY_HEAP_TYPE type);
		~QueryHeap();

		void BeginQuery(CommandContext& commandContext, D3D12_QUERY_TYPE type, UINT index) const;
		void EndQuery(CommandContext& commandContext, D3D12_QUERY_TYPE type, UINT index) const;
		void ResolveQueryData(CommandContext&	   commandContext,
							  D3D12_QUERY_TYPE	   type,
							  UINT				   startIndex,
							  UINT				   numQueries,
							  ReadBackBufferD3D12* destinationBuffer,
							  UINT64			   alignedDestinationBufferOffset) const;

		ID3D12QueryHeap* GetD3D12QueryHeap() const { return m_QueryHeap.Get(); }

	private:
		RenderDeviceD3D12* m_Device;
		UINT m_NumQueries;

		Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_QueryHeap;
	};
}