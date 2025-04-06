#include "QueryHeap.h"
#include "BufferD3D12.h"

#include <cassert>

namespace EduEngine
{
	QueryHeap::QueryHeap(RenderDeviceD3D12* device, UINT numQueries, D3D12_QUERY_HEAP_TYPE type) :
		m_Device(device),
		m_NumQueries(numQueries)
	{
		D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
		queryHeapDesc.Type = type;
		queryHeapDesc.Count = numQueries;
		queryHeapDesc.NodeMask = 0;
		
		auto hr = m_Device->GetD3D12Device()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_QueryHeap));
		THROW_IF_FAILED(hr, L"Failed to create QueryHeap");
	}

	QueryHeap::~QueryHeap()
	{
		ReleaseResourceWrapper staleHeap = {};
		staleHeap.AddPageable(std::move(m_QueryHeap));

		m_Device->SafeReleaseObject(QueueID::Direct, std::move(staleHeap));
	}

	void QueryHeap::BeginQuery(CommandContext& commandContext, D3D12_QUERY_TYPE type, UINT index) const
	{
		assert(index < m_NumQueries);
		commandContext.GetCmdList()->BeginQuery(m_QueryHeap.Get(), type, index);
	}

	void QueryHeap::EndQuery(CommandContext& commandContext, D3D12_QUERY_TYPE type, UINT index) const
	{
		assert(index < m_NumQueries);
		commandContext.GetCmdList()->EndQuery(m_QueryHeap.Get(), type, index);
	}

	void QueryHeap::ResolveQueryData(CommandContext&	  commandContext,
									 D3D12_QUERY_TYPE	  type,
									 UINT				  startIndex,
									 UINT				  numQueries,
									 ReadBackBufferD3D12* destinationBuffer,
									 UINT64				  alignedDestinationBufferOffset) const
	{
		assert(startIndex + numQueries <= m_NumQueries);

		commandContext.GetCmdList()->ResolveQueryData(
			m_QueryHeap.Get(),
			type,
			startIndex,
			numQueries,
			destinationBuffer->GetD3D12Resource(),
			alignedDestinationBufferOffset
		);
	}
}