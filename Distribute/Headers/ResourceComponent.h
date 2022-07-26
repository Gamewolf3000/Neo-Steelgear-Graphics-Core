#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#include "DescriptorAllocator.h"
#include "ResourceUploader.h"
#include "HeapAllocatorGPU.h"
#include "ResourceAllocator.h"

struct ResourceComponentMemoryInfo
{
	size_t initialMinimumHeapSize = size_t(-1);
	size_t expansionMinimumSize = size_t(-1);
	HeapAllocatorGPU* heapAllocator = nullptr;
};

enum class ViewType
{
	CBV = 0,
	SRV = 1,
	UAV = 2,
	RTV = 3,
	DSV = 4,
};

enum class HeapType
{
	OWNED,
	EXTERNAL
};

template<typename ViewDesc>
struct DescriptorAllocationInfo
{
	ViewType viewType;
	ViewDesc viewDesc;
	HeapType heapType;

	union
	{
		struct ExternalInfo
		{
			ID3D12DescriptorHeap* heap;
			size_t startIndex;
			size_t nrOfDescriptors;
		} external;

		struct OwnedInfo
		{
			size_t nrOfDescriptors;
		} owned;
	} descriptorHeapInfo;

	DescriptorAllocationInfo(ViewType type, const ViewDesc& viewDesc,
		size_t nrOfDescriptors) : viewType(type), viewDesc(viewDesc)
	{
		heapType = HeapType::OWNED;
		descriptorHeapInfo.owned.nrOfDescriptors = nrOfDescriptors;
	}

	DescriptorAllocationInfo(ViewType type, ID3D12DescriptorHeap* heap,
		size_t nrOfDescriptors, size_t startDescriptorIndex = 0) : viewType(type)
	{
		heapType = HeapType::EXTERNAL;
		descriptorHeapInfo.external.heap = heap;
		descriptorHeapInfo.external.startIndex = startDescriptorIndex;
		descriptorHeapInfo.external.nrOfDescriptors = nrOfDescriptors;
	}
};

struct ResourceIndex
{
	ResourceIdentifier allocatorIdentifier;
	size_t descriptorIndex = size_t(-1);
};

class ResourceComponent
{
protected:
	std::vector<DescriptorAllocator> descriptorAllocators;

public:
	ResourceComponent() = default;
	virtual ~ResourceComponent() = default;
	ResourceComponent(const ResourceComponent& other) = delete;
	ResourceComponent& operator=(const ResourceComponent& other) = delete;
	ResourceComponent(ResourceComponent&& other) noexcept;
	ResourceComponent& operator=(ResourceComponent&& other) noexcept;

	virtual void RemoveComponent(const ResourceIndex& indexToRemove) = 0;

	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapCBV() const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapSRV() const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapUAV() const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapRTV() const;
	virtual const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapDSV() const;

	virtual bool HasDescriptorsOfType(ViewType type) const = 0;

	virtual size_t NrOfDescriptors() const;
};