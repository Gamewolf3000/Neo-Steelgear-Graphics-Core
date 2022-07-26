#pragma once

#include <vector>
#include <stdexcept>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "HeapAllocatorGPU.h"
#include "HeapHelper.h"

struct AllowedViews
{
	bool srv = true;
	bool uav = false;
	bool rtv = false;
	bool dsv = false;
};

struct ResourceIdentifier
{
	size_t heapChunkIndex = size_t(-1);
	size_t internalIndex = size_t(-1);
};

class ResourceAllocator
{
protected:
	D3D12_RESOURCE_FLAGS CreateBindFlag();

	ID3D12Resource* AllocateResource(ID3D12Heap* heap, const D3D12_RESOURCE_DESC& desc, 
		D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clearValue,
		size_t heapOffset, ID3D12Device* device);

	AllowedViews views;
	HeapAllocatorGPU* heapAllocator;
	size_t additionalHeapChunksMinimumSize = 0;

public:
	ResourceAllocator() = default;
	virtual ~ResourceAllocator();
	ResourceAllocator(const ResourceAllocator& other) = default;
	ResourceAllocator& operator=(const ResourceAllocator& other) = default;
	ResourceAllocator(ResourceAllocator&& other) noexcept;
	ResourceAllocator& operator=(ResourceAllocator&& other) noexcept;

	void Initialize(const AllowedViews& allowedViews, 
		HeapAllocatorGPU* heapAllocatorToUse, 
		size_t minimumExpansionMemoryRequest);
};