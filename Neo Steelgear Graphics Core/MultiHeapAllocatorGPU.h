#pragma once

#include <vector>

#include "HeapAllocatorGPU.h"

class MultiHeapAllocatorGPU : public HeapAllocatorGPU
{
private:
	struct AllocatedHeap
	{
		ID3D12Heap* heap = nullptr;
		size_t size = 0;
		D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
		bool inUse = false;
	};

	std::vector<AllocatedHeap> defaultHeaps;
	std::vector<AllocatedHeap> uploadHeaps;
	std::vector<AllocatedHeap> readbackHeaps;

	std::vector<AllocatedHeap>& GetHeapVector(D3D12_HEAP_TYPE type);
	HeapChunk MakeHeapChunk(AllocatedHeap& allocation, D3D12_HEAP_TYPE heapType);

public:
	MultiHeapAllocatorGPU() = default;
	virtual ~MultiHeapAllocatorGPU();
	MultiHeapAllocatorGPU(const MultiHeapAllocatorGPU& other) = delete;
	MultiHeapAllocatorGPU& operator=(const MultiHeapAllocatorGPU& other) = delete;
	MultiHeapAllocatorGPU(MultiHeapAllocatorGPU&& other) = default;
	MultiHeapAllocatorGPU& operator=(MultiHeapAllocatorGPU&& other) = default;

	void Initialize(ID3D12Device* deviceToUse);

	virtual HeapChunk AllocateChunk(size_t minimumRequiredSize, 
		D3D12_HEAP_TYPE requiredType, D3D12_HEAP_FLAGS requiredFlags) override;
	virtual void DeallocateChunk(HeapChunk& chunk) override;


};