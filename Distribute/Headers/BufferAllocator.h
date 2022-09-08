#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <vector>
#include <utility>

#include "ResourceAllocator.h"
#include "D3DPtr.h"
#include "HeapHelper.h"
#include "ResourceUploader.h"

struct BufferInfo
{
	size_t alignment = size_t(-1);
	size_t elementSize = size_t(-1);
};

struct BufferHandle
{
	ID3D12Resource* resource;
	size_t startOffset;
	size_t nrOfElements;
};

class BufferAllocator : public ResourceAllocator
{
private:

	struct BufferEntry
	{
		size_t nrOfElements = 0;
	};

	struct MemoryChunk
	{
		HeapChunk heapChunk;
		HeapHelper<BufferEntry> buffers;
		D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
		D3DPtr<ID3D12Resource> resource;
		unsigned char* mappedStart = nullptr;
	};

	ID3D12Device* device = nullptr;
	std::vector<MemoryChunk> memoryChunks;
	BufferInfo bufferInfo;

	ID3D12Resource* AllocateResource(size_t size, ID3D12Heap* heap,
		size_t startOffset, D3D12_RESOURCE_STATES initialState);

	ResourceIdentifier GetAvailableHeapIndex(size_t nrOfElements);

public:
	BufferAllocator() = default;
	virtual ~BufferAllocator() = default;
	BufferAllocator(const BufferAllocator& other) = delete;
	BufferAllocator& operator=(const BufferAllocator& other) = delete;
	BufferAllocator(BufferAllocator&& other) noexcept;
	BufferAllocator& operator=(BufferAllocator&& other) noexcept;

	void Initialize(const BufferInfo& bufferInfoToUse, ID3D12Device* deviceToUse,
		bool mappedUpdateable, const AllowedViews& allowedViews, size_t initialHeapSize,
		size_t minimumExpansionMemoryRequest, HeapAllocatorGPU* heapAllocatorToUse);

	ResourceIdentifier AllocateBuffer(size_t nrOfElements);
	void DeallocateBuffer(const ResourceIdentifier& identifier);

	void CreateTransitionBarrier(D3D12_RESOURCE_STATES newState, 
		std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		std::optional<D3D12_RESOURCE_STATES> assumedInitialState = std::nullopt);

	unsigned char* GetMappedPtr(const ResourceIdentifier& identifier);
	BufferHandle GetHandle(const ResourceIdentifier& identifier);
	const BufferHandle GetHandle(const ResourceIdentifier& identifier) const;
	size_t GetElementSize();
	size_t GetElementAlignment();
	D3D12_RESOURCE_STATES GetCurrentState();

	void UpdateMappedBuffer(const ResourceIdentifier& identifier, void* data); // Map/Unmap method
};