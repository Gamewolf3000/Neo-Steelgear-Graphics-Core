#pragma once

#include <optional>

#include <d3d12.h>

struct HeapChunk
{
	D3D12_HEAP_TYPE heapType;
	D3D12_HEAP_FLAGS heapFlags;
	ID3D12Heap* heap = nullptr;
	size_t startOffset = size_t(-1);
	size_t endOffset = 0;
};

//enum class ExpansionType
//{
//	ADDITATIVE_INCREMENT,
//	MULTIPLICATIVE_INCREMENT
//};
//
//struct ExpansionInfo
//{
//	ExpansionType type;
//
//	union
//	{
//		size_t additative = 0;
//		double multiplicative;
//	} increment;
//
//	ExpansionInfo() = default;
//
//	ExpansionInfo(size_t additativeIncrement) : type(ExpansionType::ADDITATIVE_INCREMENT)
//	{
//		increment.additative = additativeIncrement;
//	}
//
//	ExpansionInfo(double multiplicativeIncrement) : type(ExpansionType::MULTIPLICATIVE_INCREMENT)
//	{
//		increment.multiplicative = multiplicativeIncrement;
//	}
//};
//
//struct MemoryAllocationInfo
//{
//	size_t startMemorySize = 0;
//	ExpansionInfo expansionInfo;
//};

class HeapAllocatorGPU
{
protected:
	ID3D12Device* device;

	ID3D12Heap* CreateHeap(size_t heapSize, D3D12_HEAP_TYPE heapType,
		D3D12_HEAP_FLAGS heapFlags);

public:
	HeapAllocatorGPU() = default;
	virtual ~HeapAllocatorGPU() = default;
	HeapAllocatorGPU(const HeapAllocatorGPU& other) = delete;
	HeapAllocatorGPU& operator=(const HeapAllocatorGPU& other) = delete;
	HeapAllocatorGPU(HeapAllocatorGPU&& other) = default;
	HeapAllocatorGPU& operator=(HeapAllocatorGPU&& other) = default;

	virtual HeapChunk AllocateChunk(size_t minimumRequiredSize, 
		D3D12_HEAP_TYPE requiredType, D3D12_HEAP_FLAGS requiredFlags) = 0;
	virtual void DeallocateChunk(HeapChunk& chunk) = 0;
};