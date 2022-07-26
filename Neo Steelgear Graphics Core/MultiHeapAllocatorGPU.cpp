#include "MultiHeapAllocatorGPU.h"

#include <stdexcept>

std::vector<MultiHeapAllocatorGPU::AllocatedHeap>& 
MultiHeapAllocatorGPU::GetHeapVector(D3D12_HEAP_TYPE type)
{
	switch (type)
	{
	case D3D12_HEAP_TYPE_DEFAULT:
		return defaultHeaps;
		break;
	case D3D12_HEAP_TYPE_UPLOAD:
		return uploadHeaps;
		break;
	case D3D12_HEAP_TYPE_READBACK:
		return readbackHeaps;
		break;
	default:
		throw std::runtime_error("Incorrect heap type requested from multi heap GPU allocator");
		break;
	}
}

HeapChunk MultiHeapAllocatorGPU::MakeHeapChunk(AllocatedHeap& allocation,
	D3D12_HEAP_TYPE heapType)
{
	HeapChunk toReturn;
	toReturn.heap = allocation.heap;
	toReturn.heapFlags = allocation.heapFlags;
	toReturn.heapType = heapType;
	toReturn.startOffset = 0;
	toReturn.endOffset = allocation.size;
	allocation.inUse = true;
	return toReturn;
}

MultiHeapAllocatorGPU::~MultiHeapAllocatorGPU()
{
	for (auto& allocation : defaultHeaps)
		allocation.heap->Release();

	for (auto& allocation : uploadHeaps)
		allocation.heap->Release();

	for (auto& allocation : readbackHeaps)
		allocation.heap->Release();
}

void MultiHeapAllocatorGPU::Initialize(ID3D12Device* deviceToUse)
{
	device = deviceToUse;
}

HeapChunk MultiHeapAllocatorGPU::AllocateChunk(size_t minimumRequiredSize,
    D3D12_HEAP_TYPE requiredType, D3D12_HEAP_FLAGS requiredFlags)
{
	auto& vector = GetHeapVector(requiredType);
	AllocatedHeap* bestAllocation = nullptr;
	size_t bestSize = size_t(-1);

	for (auto& allocatedHeap : vector)
	{
		if (!allocatedHeap.inUse && allocatedHeap.heapFlags == requiredFlags &&
			allocatedHeap.size >= minimumRequiredSize && allocatedHeap.size < bestSize)
		{
			bestAllocation = &allocatedHeap;
			bestSize = allocatedHeap.size;
		}
	}

	if (bestAllocation == nullptr) // temp, check if nullptr
	{
		AllocatedHeap toAdd;
		toAdd.heap = CreateHeap(minimumRequiredSize, requiredType, requiredFlags);
		toAdd.heapFlags = requiredFlags;
		toAdd.size = minimumRequiredSize;
		vector.push_back(toAdd);
		bestAllocation = &vector.back();
	}

	return MakeHeapChunk(*bestAllocation, requiredType);
}

void MultiHeapAllocatorGPU::DeallocateChunk(HeapChunk& chunk)
{
	auto& vector = GetHeapVector(chunk.heapType);

	for (auto& allocatedHeap : vector)
	{
		if (allocatedHeap.heap == chunk.heap)
		{
			allocatedHeap.inUse = false;
			chunk = HeapChunk();
		}
	}
}
