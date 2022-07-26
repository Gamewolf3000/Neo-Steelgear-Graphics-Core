#include "ResourceAllocator.h"


D3D12_RESOURCE_FLAGS ResourceAllocator::CreateBindFlag()
{
	D3D12_RESOURCE_FLAGS toReturn = D3D12_RESOURCE_FLAG_NONE;

	if (!views.srv && views.dsv)
		toReturn |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	if (views.uav)
		toReturn |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	if (views.rtv)
		toReturn |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (views.dsv)
		toReturn |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	return toReturn;
}
//
//template<typename InternalAllocationInfo>
//inline AllocationIdentifier ResourceAllocator::AllocateResource(
//	const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState,
//	const D3D12_CLEAR_VALUE* clearValue, ID3D12Device* device)
//{
//	AllocationIdentifier toReturn;
//	auto resourceInfo = device->GetResourceAllocationInfo(0, 1, &desc);
//
//	for (size_t i = 0; i < heapChunks.size(); ++i)
//	{
//		toReturn.internalIndex = heapChunks[i].heapHelper.AllocateChunk(
//			static_cast<size_t>(resourceInfo.SizeInBytes),
//			AllocationStrategy::FIRST_FIT, static_cast<size_t>(resourceInfo.Alignment));
//
//		if (toReturn.internalIndex != size_t(-1))
//		{
//			toReturn.heapChunkIndex = i;
//			break;
//		}
//	}
//
//	if (toReturn.heapChunkIndex == size_t(-1)) // Did not fit into existing heap(s)
//	{
//		size_t sizeToRequest = resourceInfo.SizeInBytes <= additionalHeapChunksMinimumSize ?
//			additionalHeapChunksMinimumSize : resourceInfo.SizeInBytes;
//		AllocateHeapChunk(sizeToRequest, heapChunks[0].heapChunk.heapType,
//			heapChunks[0].heapChunk.heapFlags);
//		toReturn.heapChunkIndex = heapChunks.size() - 1;
//		toReturn.internalIndex = heapChunks.back().heapHelper.AllocateChunk(
//			static_cast<size_t>(resourceInfo.SizeInBytes),
//			AllocationStrategy::FIRST_FIT, static_cast<size_t>(resourceInfo.Alignment));
//	}
//
//	//FORTSÄTT HÄR! HAR VART DEN SKA SKAPAS NU MEN MÅSTE FAKTISKT SKAPA DEN SAMT SE TILL ATT DEN KOMMER TILLBAKA TILL CALLERN
//
//	ID3D12Resource* toReturn;
//
//	HRESULT hr = device->CreatePlacedResource(heapData.heap, heapOffset,
//		&desc, initialState, clearValue, IID_PPV_ARGS(&toReturn));
//
//	if (FAILED(hr))
//	{
//		auto error = "Failed to allocate resource in heap in resource allocator";
//		throw std::runtime_error(error);
//	}
//
//	return toReturn;
//}

ID3D12Resource* ResourceAllocator::AllocateResource(ID3D12Heap* heap,
	const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES initialState,
	const D3D12_CLEAR_VALUE* clearValue, size_t heapOffset, ID3D12Device* device)
{
	ID3D12Resource* toReturn;

	HRESULT hr = device->CreatePlacedResource(heap, heapOffset,
		&desc, initialState, clearValue, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
	{
		auto error = "Failed to allocate resource in heap in resource allocator";
		throw std::runtime_error(error);
	}

	return toReturn;
}

ResourceAllocator::~ResourceAllocator()
{
	// EMPTY
}

ResourceAllocator::ResourceAllocator(ResourceAllocator&& other) noexcept :
	views(other.views)
{
	other.views = AllowedViews();
}

ResourceAllocator& ResourceAllocator::operator=(ResourceAllocator&& other) noexcept
{
	if (this != &other)
	{
		views = other.views;
		other.views = AllowedViews();
	}

	return *this;
}

void ResourceAllocator::Initialize(const AllowedViews& allowedViews,
	HeapAllocatorGPU* heapAllocatorToUse, size_t minimumExpansionMemoryRequest)
{
	views = allowedViews;
	heapAllocator = heapAllocatorToUse;
	additionalHeapChunksMinimumSize = minimumExpansionMemoryRequest;
}