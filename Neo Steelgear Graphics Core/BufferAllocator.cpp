#include "BufferAllocator.h"

#include <stdexcept>

ID3D12Resource* BufferAllocator::AllocateResource(size_t size, ID3D12Device* device)
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

	return ResourceAllocator::AllocateResource(desc, currentState, nullptr,
		heapData.startOffset, device);
}

BufferAllocator::BufferAllocator(BufferAllocator&& other) noexcept :
	ResourceAllocator(std::move(other)), resource(std::move(other.resource)),
	mappedStart(other.mappedStart), bufferInfo(other.bufferInfo),
	buffers(std::move(other.buffers)), currentState(other.currentState)
{
	other.mappedStart = nullptr;
	other.bufferInfo = BufferInfo();
	other.currentState = D3D12_RESOURCE_STATE_COMMON;
}

BufferAllocator& BufferAllocator::operator=(BufferAllocator&& other) noexcept
{
	if (this != &other)
	{
		ResourceAllocator::operator=(std::move(other));
		resource = std::move(other.resource);
		mappedStart = other.mappedStart;
		other.mappedStart = nullptr;
		bufferInfo = std::move(other.bufferInfo);
		other.bufferInfo = BufferInfo();
		buffers = std::move(other.buffers);
		currentState = other.currentState;
		other.currentState = D3D12_RESOURCE_STATE_COMMON;
	}

	return *this;
}

void BufferAllocator::Initialize(const BufferInfo& bufferInfoToUse,
	ID3D12Device* device, bool mappedUpdateable,
	const AllowedViews& allowedViews, ID3D12Heap* heap, size_t startOffset,
	size_t endOffset)
{
	ResourceAllocator::Initialize(allowedViews);
	bufferInfo = bufferInfoToUse;
	bufferInfo.elementSize = ((bufferInfo.elementSize +
		(bufferInfo.alignment - 1)) & ~(bufferInfo.alignment - 1));
	buffers.Initialize(endOffset - startOffset);
	currentState = D3D12_RESOURCE_STATE_COMMON;
	heapData.heapOwned = false;
	heapData.heap = heap;
	heapData.startOffset = startOffset;
	heapData.endOffset = endOffset;
	resource = AllocateResource(endOffset - startOffset, device);

	if (mappedUpdateable)
	{
		D3D12_RANGE nothing = { 0, 0 }; // We only write, we do not read
		HRESULT hr = resource->Map(0, &nothing, reinterpret_cast<void**>(&mappedStart));
		if (FAILED(hr))
			throw std::runtime_error("Could not map allocated buffer");
	}
}

void BufferAllocator::Initialize(const BufferInfo& bufferInfoToUse,
	ID3D12Device* device, bool mappedUpdateable,
	const AllowedViews& allowedViews, size_t heapSize)
{
	ResourceAllocator::Initialize(allowedViews);
	bufferInfo = bufferInfoToUse;
	bufferInfo.elementSize = ((bufferInfo.elementSize + 
		(bufferInfo.alignment - 1)) & ~(bufferInfo.alignment - 1));
	buffers.Initialize(heapSize);
	currentState = mappedUpdateable ? D3D12_RESOURCE_STATE_GENERIC_READ :
		D3D12_RESOURCE_STATE_COMMON;
	heapData.heapOwned = true;
	heapData.heap = AllocateHeap(heapSize, mappedUpdateable, 
		D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS, device);
	heapData.startOffset = 0;
	heapData.endOffset = heapSize;
	resource = AllocateResource(heapSize, device);

	if (mappedUpdateable)
	{
		D3D12_RANGE nothing = { 0, 0 }; // We only write, we do not read
		HRESULT hr = resource->Map(0, &nothing, reinterpret_cast<void**>(&mappedStart));
		if (FAILED(hr))
			throw std::runtime_error("Could not map allocated buffer");
	}
}

size_t BufferAllocator::AllocateBuffer(size_t nrOfElements)
{
	size_t bufferEntryIndex = buffers.AllocateChunk(
		bufferInfo.elementSize * nrOfElements,
		AllocationStrategy::FIRST_FIT, bufferInfo.alignment);

	if (bufferEntryIndex != size_t(-1))
		buffers[bufferEntryIndex].nrOfElements = nrOfElements;

	return bufferEntryIndex;
}

void BufferAllocator::DeallocateBuffer(size_t index)
{
	buffers.DeallocateChunk(index);
}

D3D12_RESOURCE_BARRIER BufferAllocator::CreateTransitionBarrier(
	D3D12_RESOURCE_STATES newState, D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	D3D12_RESOURCE_BARRIER toReturn;

	toReturn.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toReturn.Flags = flag;
	toReturn.Transition.pResource = resource;
	toReturn.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	toReturn.Transition.StateBefore = currentState;
	toReturn.Transition.StateAfter = newState;

	currentState = newState;

	return toReturn;
}

unsigned char* BufferAllocator::GetMappedPtr()
{
	return mappedStart;
}

BufferHandle BufferAllocator::GetHandle(size_t index)
{
	BufferHandle toReturn;
	toReturn.resource = resource;
	toReturn.startOffset = buffers.GetStartOfChunk(index);
	toReturn.nrOfElements = buffers[index].nrOfElements;

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
	return currentState;
}

void BufferAllocator::UpdateMappedBuffer(size_t index, void* data)
{
	auto& entry = buffers[index];
	void* resourceStart = mappedStart + buffers.GetStartOfChunk(index);
	memcpy(resourceStart, data, bufferInfo.elementSize * entry.nrOfElements);
}