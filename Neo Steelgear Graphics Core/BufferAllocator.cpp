#include "BufferAllocator.h"

#include <stdexcept>

ID3D12Resource* BufferAllocator::AllocateResource(size_t size, ID3D12Heap* heap,
	size_t startOffset, D3D12_RESOURCE_STATES initialState)
{
	D3D12_RESOURCE_DESC desc;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = CreateBindFlag();

	return ResourceAllocator::AllocateResource(heap, desc, 
		initialState, nullptr, startOffset, device);
}

ResourceIdentifier BufferAllocator::GetAvailableHeapIndex(size_t nrOfElements)
{
	ResourceIdentifier toReturn;

	for (size_t i = 0; i < memoryChunks.size(); ++i)
	{
		toReturn.internalIndex = memoryChunks[i].buffers.AllocateChunk(
			nrOfElements * bufferInfo.elementSize, AllocationStrategy::FIRST_FIT,
			bufferInfo.alignment);

		if (toReturn.internalIndex != size_t(-1))
		{
			toReturn.heapChunkIndex = i;
			break;
		}
	}

	if (toReturn.heapChunkIndex == size_t(-1)) // No fit in existing chunks, need to expand
	{
		MemoryChunk newChunk;
		size_t minimumSize = std::max<size_t>(additionalHeapChunksMinimumSize,
			nrOfElements * bufferInfo.elementSize);
		newChunk.heapChunk = heapAllocator->AllocateChunk(minimumSize,
			memoryChunks[0].heapChunk.heapType, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);

		size_t heapSize = newChunk.heapChunk.endOffset - newChunk.heapChunk.startOffset;
		newChunk.buffers.Initialize(heapSize, newChunk.heapChunk.startOffset);

		D3D12_RESOURCE_STATES initialState = 
			memoryChunks[0].heapChunk.heapType == D3D12_HEAP_TYPE_UPLOAD ?
			D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
		newChunk.currentState = initialState;

		newChunk.resource = AllocateResource(heapSize, newChunk.heapChunk.heap,
			newChunk.heapChunk.startOffset, initialState);

		if (initialState == D3D12_RESOURCE_STATE_GENERIC_READ)
		{
			D3D12_RANGE nothing = { 0, 0 }; // We only write, we do not read
			HRESULT hr = newChunk.resource->Map(0, &nothing,
				reinterpret_cast<void**>(&newChunk.mappedStart));
			if (FAILED(hr))
				throw std::runtime_error("Could not map allocated buffer");
		}

		memoryChunks.push_back(std::move(newChunk));

		toReturn.internalIndex = memoryChunks.back().buffers.AllocateChunk(
			nrOfElements * bufferInfo.elementSize, AllocationStrategy::FIRST_FIT,
			bufferInfo.alignment);
		toReturn.heapChunkIndex = memoryChunks.size() - 1;

		if (toReturn.internalIndex == size_t(-1))
			throw std::runtime_error("Failed to expand memory in buffer allocator");
	}

	return toReturn;
}

BufferAllocator::BufferAllocator(BufferAllocator&& other) noexcept :
	ResourceAllocator(std::move(other)), device(other.device),
	memoryChunks(std::move(other.memoryChunks)), bufferInfo(other.bufferInfo)
{
	other.device = nullptr;
	other.bufferInfo = BufferInfo();
}

BufferAllocator& BufferAllocator::operator=(BufferAllocator&& other) noexcept
{
	if (this != &other)
	{
		ResourceAllocator::operator=(std::move(other));
		device = other.device;
		other.device = nullptr;
		memoryChunks = std::move(other.memoryChunks);
		bufferInfo = std::move(other.bufferInfo);
		other.bufferInfo = BufferInfo();
	}

	return *this;
}

void BufferAllocator::Initialize(const BufferInfo& bufferInfoToUse, 
	ID3D12Device* deviceToUse, bool mappedUpdateable, 
	const AllowedViews& allowedViews, size_t initialHeapSize,
	size_t minimumExpansionMemoryRequest, HeapAllocatorGPU* heapAllocatorToUse)
{
	ResourceAllocator::Initialize(allowedViews, heapAllocatorToUse,
		minimumExpansionMemoryRequest);
	bufferInfo = bufferInfoToUse;
	bufferInfo.elementSize = ((bufferInfo.elementSize + 
		(bufferInfo.alignment - 1)) & ~(bufferInfo.alignment - 1));
	device = deviceToUse;

	MemoryChunk initialChunk;
	initialChunk.heapChunk = heapAllocator->AllocateChunk(initialHeapSize,
		mappedUpdateable ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT,
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
	
	size_t heapSize = initialChunk.heapChunk.endOffset -
		initialChunk.heapChunk.startOffset;
	initialChunk.buffers.Initialize(heapSize, initialChunk.heapChunk.startOffset);

	D3D12_RESOURCE_STATES initialState = mappedUpdateable ? 
		D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;
	initialChunk.currentState = initialState;

	initialChunk.resource = AllocateResource(heapSize, initialChunk.heapChunk.heap,
		initialChunk.heapChunk.startOffset, initialState);

	if (initialState == D3D12_RESOURCE_STATE_GENERIC_READ)
	{
		D3D12_RANGE nothing = { 0, 0 }; // We only write, we do not read
		HRESULT hr = initialChunk.resource->Map(0, &nothing, 
			reinterpret_cast<void**>(&initialChunk.mappedStart));
		if (FAILED(hr))
			throw std::runtime_error("Could not map allocated buffer");
	}

	memoryChunks.push_back(std::move(initialChunk));
}

ResourceIdentifier BufferAllocator::AllocateBuffer(size_t nrOfElements)
{
	ResourceIdentifier toReturn = GetAvailableHeapIndex(nrOfElements);

	auto& bufferVector = memoryChunks[toReturn.heapChunkIndex].buffers;
	bufferVector[toReturn.internalIndex].nrOfElements = nrOfElements;

	return toReturn;
}

void BufferAllocator::DeallocateBuffer(const ResourceIdentifier& identifier)
{
	auto& bufferVector = memoryChunks[identifier.heapChunkIndex].buffers;
	bufferVector.DeallocateChunk(identifier.internalIndex);
}

void BufferAllocator::CreateTransitionBarrier(D3D12_RESOURCE_STATES newState,
	std::vector<D3D12_RESOURCE_BARRIER>& barriers, D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	for (auto& memoryChunk : memoryChunks)
	{
		D3D12_RESOURCE_BARRIER toAdd;

		toAdd.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		toAdd.Flags = flag;
		toAdd.Transition.pResource = memoryChunk.resource;
		toAdd.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		toAdd.Transition.StateBefore = memoryChunk.currentState;
		toAdd.Transition.StateAfter = newState;

		memoryChunk.currentState = newState;
		barriers.push_back(toAdd);
	}
}

unsigned char* BufferAllocator::GetMappedPtr(const ResourceIdentifier& identifier)
{
	auto& memoryChunk = memoryChunks[identifier.heapChunkIndex];
	auto toReturn = memoryChunk.mappedStart + 
		memoryChunk.buffers.GetStartOfChunk(identifier.internalIndex);

	return toReturn;
}

BufferHandle BufferAllocator::GetHandle(const ResourceIdentifier& identifier)
{
	auto& memoryChunk = memoryChunks[identifier.heapChunkIndex];

	BufferHandle toReturn;
	toReturn.resource = memoryChunk.resource;
	toReturn.startOffset = memoryChunk.buffers.GetStartOfChunk(identifier.internalIndex);
	toReturn.nrOfElements = memoryChunk.buffers[identifier.internalIndex].nrOfElements;

	return toReturn;
}

const BufferHandle BufferAllocator::GetHandle(const ResourceIdentifier& identifier) const
{
	auto& memoryChunk = memoryChunks[identifier.heapChunkIndex];

	BufferHandle toReturn;
	toReturn.resource = memoryChunk.resource;
	toReturn.startOffset = memoryChunk.buffers.GetStartOfChunk(identifier.internalIndex);
	toReturn.nrOfElements = memoryChunk.buffers[identifier.internalIndex].nrOfElements;

	return toReturn;
}

size_t BufferAllocator::GetElementSize()
{
	return bufferInfo.elementSize;
}

size_t BufferAllocator::GetElementAlignment()
{
	return bufferInfo.alignment;
}

D3D12_RESOURCE_STATES BufferAllocator::GetCurrentState()
{
	return memoryChunks[0].currentState;
}

void BufferAllocator::UpdateMappedBuffer(const ResourceIdentifier& identifier, void* data)
{
	auto& memoryChunk = memoryChunks[identifier.heapChunkIndex];
	void* resourceStart = memoryChunk.mappedStart + 
		memoryChunk.buffers.GetStartOfChunk(identifier.internalIndex);

	auto& bufferEntry = memoryChunk.buffers[identifier.internalIndex];
	memcpy(resourceStart, data, bufferInfo.elementSize * bufferEntry.nrOfElements);
}