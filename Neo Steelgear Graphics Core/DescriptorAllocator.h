#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include "StableVector.h"

struct DescriptorInfo
{
	size_t descriptorSize = 0;
	D3D12_DESCRIPTOR_HEAP_TYPE type;
};

class DescriptorAllocator
{
private:
	struct DescriptorHeapData
	{
		bool heapOwned = false;
		ID3D12DescriptorHeap* heap = nullptr;
		DescriptorInfo descriptorInfo;
		size_t startIndex = size_t(-1);
		size_t endIndex = size_t(-1);
	} heapData;

	struct StoredDescriptor
	{
		// Empty
	};

	ID3D12Device* device = nullptr;
	StableVector<StoredDescriptor> descriptors;

	ID3D12DescriptorHeap* AllocateHeap(size_t nrOfDescriptors);
	size_t GetFreeDescriptorIndex();
	bool AllocationHelper(size_t& index, D3D12_CPU_DESCRIPTOR_HANDLE& handle);

public:
	DescriptorAllocator() = default;
	~DescriptorAllocator();
	DescriptorAllocator(const DescriptorAllocator& other) = delete;
	DescriptorAllocator& operator=(const DescriptorAllocator& other) = delete;
	DescriptorAllocator(DescriptorAllocator&& other) noexcept;
	DescriptorAllocator& operator=(DescriptorAllocator&& other) noexcept;

	void Initialize(const DescriptorInfo& descriptorInfo, ID3D12Device* deviceToUse,
		ID3D12DescriptorHeap* heap, size_t startIndex, size_t nrOfDescriptors);
	void Initialize(const DescriptorInfo& descriptorInfo, ID3D12Device* deviceToUse,
		size_t nrOfDescriptors);

	size_t AllocateSRV(ID3D12Resource* resource,
		D3D12_SHADER_RESOURCE_VIEW_DESC* desc = nullptr);
	size_t AllocateDSV(ID3D12Resource* resource,
		D3D12_DEPTH_STENCIL_VIEW_DESC* desc = nullptr);
	size_t AllocateRTV(ID3D12Resource* resource,
		D3D12_RENDER_TARGET_VIEW_DESC* desc = nullptr);
	size_t AllocateUAV(ID3D12Resource* resource,
		D3D12_UNORDERED_ACCESS_VIEW_DESC* desc = nullptr,
		ID3D12Resource* counterResource = nullptr);
	size_t AllocateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc = nullptr);

	void DeallocateDescriptor(size_t index);

	D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(size_t index);
};