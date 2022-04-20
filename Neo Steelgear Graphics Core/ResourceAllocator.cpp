#include "ResourceAllocator.h"

#include <stdexcept>

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

ID3D12Heap* ResourceAllocator::AllocateHeap(size_t size, bool uploadHeap,
	D3D12_HEAP_FLAGS flags, ID3D12Device* device)
{
	D3D12_HEAP_TYPE heapType = uploadHeap ? D3D12_HEAP_TYPE_UPLOAD :
		D3D12_HEAP_TYPE_DEFAULT;

	D3D12_HEAP_DESC desc;
	desc.SizeInBytes = size;
	desc.Properties.Type = heapType;
	desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	desc.Properties.CreationNodeMask = 0;
	desc.Properties.VisibleNodeMask = 0;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Flags = flags;

	ID3D12Heap* toReturn;
	HRESULT hr = device->CreateHeap(&desc, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
	{
		auto error = "Failed to allocate heap in resource allocator";
		throw std::runtime_error(error);
	}

	return toReturn;
}

ID3D12Resource* ResourceAllocator::AllocateResource(const D3D12_RESOURCE_DESC& desc,
	D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clearValue,
	size_t heapOffset, ID3D12Device* device)
{
	ID3D12Resource* toReturn;

	HRESULT hr = device->CreatePlacedResource(heapData.heap, heapOffset,
		&desc, initialState, clearValue, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
	{
		auto error = "Failed to allocate resource in heap in resource allocator";
		throw std::runtime_error(error);
	}

	return toReturn;
}

ResourceAllocator::ResourceAllocator(ResourceAllocator&& other) noexcept :
	heapData(other.heapData), views(other.views) 
{
	other.heapData = ResourceHeapData();
	other.views = AllowedViews();
}

ResourceAllocator& ResourceAllocator::operator=(ResourceAllocator&& other) noexcept
{
	if (this != &other)
	{
		heapData = other.heapData;
		other.heapData = ResourceHeapData();
		views = other.views;
		other.views = AllowedViews();
	}

	return *this;
}

void ResourceAllocator::Initialize(const AllowedViews& allowedViews)
{
	views = allowedViews;
}
