#include "HeapAllocatorGPU.h"

#include <stdexcept>

ID3D12Heap* HeapAllocatorGPU::CreateHeap(size_t heapSize,
    D3D12_HEAP_TYPE heapType, D3D12_HEAP_FLAGS heapFlags)
{
	D3D12_HEAP_DESC desc;
	desc.SizeInBytes = heapSize;
	desc.Properties.Type = heapType;
	desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	desc.Properties.CreationNodeMask = 0;
	desc.Properties.VisibleNodeMask = 0;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Flags = heapFlags;

	ID3D12Heap* toReturn = nullptr;
	HRESULT hr = device->CreateHeap(&desc, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
	{
		auto error = "Failed to allocate heap in GPU heap allocator";
		throw std::runtime_error(error);
	}

	return toReturn;
}