#include "TextureAllocator.h"

D3D12_RESOURCE_DESC TextureAllocator::CreateTextureDesc(
	const TextureAllocationInfo& info,
	std::optional<D3D12_RESOURCE_FLAGS> replacementBindings)
{
	D3D12_RESOURCE_DESC toReturn;

	toReturn.Dimension = info.textureType;
	toReturn.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	toReturn.Width = static_cast<UINT>(info.dimensions.width);
	toReturn.Height = static_cast<UINT>(info.dimensions.height);
	toReturn.DepthOrArraySize = static_cast<UINT16>(info.dimensions.depthOrArraySize);
	toReturn.MipLevels = static_cast<UINT16>(info.mipLevels);
	toReturn.Format = info.format;
	toReturn.SampleDesc.Count = info.sampleCount;
	toReturn.SampleDesc.Quality = info.sampleQuality;
	toReturn.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	toReturn.Flags = replacementBindings.has_value() ?
		replacementBindings.value() : CreateBindFlag();

	return toReturn;
}

ResourceIdentifier TextureAllocator::GetAvailableHeapIndex(D3D12_RESOURCE_ALLOCATION_INFO allocationInfo)
{
	ResourceIdentifier toReturn;

	for (size_t i = 0; i < memoryChunks.size(); ++i)
	{
		toReturn.internalIndex = memoryChunks[i].textures.AllocateChunk(
			static_cast<size_t>(allocationInfo.SizeInBytes),
			AllocationStrategy::FIRST_FIT, static_cast<size_t>(allocationInfo.Alignment));
	
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
			allocationInfo.SizeInBytes + allocationInfo.Alignment);
		newChunk.heapChunk = heapAllocator->AllocateChunk(minimumSize,
			D3D12_HEAP_TYPE_DEFAULT, memoryChunks[0].heapChunk.heapFlags);

		size_t heapSize = newChunk.heapChunk.endOffset -
			newChunk.heapChunk.startOffset;
		newChunk.textures.Initialize(heapSize, newChunk.heapChunk.startOffset);
		memoryChunks.push_back(std::move(newChunk));

		toReturn.internalIndex = memoryChunks.back().textures.AllocateChunk(
			static_cast<size_t>(allocationInfo.SizeInBytes),
			AllocationStrategy::FIRST_FIT, static_cast<size_t>(allocationInfo.Alignment));
		toReturn.heapChunkIndex = memoryChunks.size() - 1;

		if (toReturn.internalIndex == size_t(-1))
			throw std::runtime_error("Failed to expand memory in texture allocator");
	}

	return toReturn;
}

TextureAllocator::~TextureAllocator()
{
	for (auto& memoryChunk : memoryChunks)
	{
		memoryChunk.textures.ClearHeap();
		heapAllocator->DeallocateChunk(memoryChunk.heapChunk);
	}
}

TextureAllocator::TextureAllocator(TextureAllocator&& other) noexcept : 
	ResourceAllocator(std::move(other)), device(other.device),
	memoryChunks(std::move(other.memoryChunks))
{
	other.device = nullptr;
}

TextureAllocator& TextureAllocator::operator=(TextureAllocator&& other) noexcept
{
	if (this != &other)
	{
		ResourceAllocator::operator=(std::move(other));
		device = other.device;
		other.device = nullptr;
		memoryChunks = std::move(other.memoryChunks);
	}

	return *this;
}

void TextureAllocator::Initialize(ID3D12Device* deviceToUse,
	const AllowedViews& allowedViews, size_t initialHeapSize,
	size_t minimumExpansionMemoryRequest, HeapAllocatorGPU* heapAllocatorToUse)
{
	ResourceAllocator::Initialize(allowedViews, heapAllocatorToUse, 
		minimumExpansionMemoryRequest);
	device = deviceToUse;

	MemoryChunk initialChunk;

	D3D12_HEAP_FLAGS heapFlag = allowedViews.dsv || allowedViews.rtv ?
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES :
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	initialChunk.heapChunk = heapAllocator->AllocateChunk(
		initialHeapSize, D3D12_HEAP_TYPE_DEFAULT, heapFlag);

	size_t heapSize = initialChunk.heapChunk.endOffset - 
		initialChunk.heapChunk.startOffset;
	initialChunk.textures.Initialize(heapSize, initialChunk.heapChunk.startOffset);
	memoryChunks.push_back(std::move(initialChunk));
}

void TextureAllocator::ResetAllocator()
{
	for (size_t i = 1; i < memoryChunks.size(); ++i)
	{
		memoryChunks[i].textures.ClearHeap();
		heapAllocator->DeallocateChunk(memoryChunks[i].heapChunk);
	}

	memoryChunks[0].textures.ClearHeap();
	memoryChunks.resize(1); // Keep initial chunk
}

ResourceIdentifier TextureAllocator::AllocateTexture(const TextureAllocationInfo& info,
	std::optional<D3D12_RESOURCE_FLAGS> replacementBindings)
{
	D3D12_RESOURCE_DESC desc = CreateTextureDesc(info, replacementBindings);
	auto allocationInfo = device->GetResourceAllocationInfo(0, 1, &desc);

	ResourceIdentifier toReturn = GetAvailableHeapIndex(allocationInfo);

	const D3D12_CLEAR_VALUE* clearValue = info.clearValue.has_value() ?
		&info.clearValue.value() : nullptr;
	
	auto& textureVector = memoryChunks[toReturn.heapChunkIndex].textures;
	auto& textureEntry = textureVector[toReturn.internalIndex];

	textureEntry.resource = ResourceAllocator::AllocateResource(
		memoryChunks[toReturn.heapChunkIndex].heapChunk.heap, desc, D3D12_RESOURCE_STATE_COMMON,
		clearValue, textureVector.GetStartOfChunk(toReturn.internalIndex), device);
	textureEntry.currentState = D3D12_RESOURCE_STATE_COMMON;
	textureEntry.dimensions = info.dimensions;
	textureEntry.texelSize = info.texelSize;
	textureEntry.clearValue = clearValue != nullptr ? std::make_optional(*clearValue) : std::nullopt;

	return toReturn;
}

void TextureAllocator::DeallocateTexture(const ResourceIdentifier& identifier)
{
	auto& textureVector = memoryChunks[identifier.heapChunkIndex].textures;
	textureVector[identifier.internalIndex].resource->Release();
	textureVector[identifier.internalIndex].resource = nullptr;
	textureVector.DeallocateChunk(identifier.internalIndex);
}

TextureHandle TextureAllocator::GetHandle(const ResourceIdentifier& identifier)
{
	const auto& textureEntry = 
		memoryChunks[identifier.heapChunkIndex].textures[identifier.internalIndex];

	TextureHandle toReturn;
	toReturn.resource = textureEntry.resource;
	toReturn.dimensions = textureEntry.dimensions;

	return toReturn;
}

D3D12_RESOURCE_STATES TextureAllocator::GetCurrentState(const ResourceIdentifier& identifier)
{
	return memoryChunks[identifier.heapChunkIndex].textures[identifier.internalIndex].currentState;
}

D3D12_RESOURCE_BARRIER TextureAllocator::CreateTransitionBarrier(
	const ResourceIdentifier& identifier, D3D12_RESOURCE_STATES newState,
	D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	auto& textureEntry = 
		memoryChunks[identifier.heapChunkIndex].textures[identifier.internalIndex];

	D3D12_RESOURCE_BARRIER toReturn;

	toReturn.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toReturn.Flags = flag;
	toReturn.Transition.pResource = textureEntry.resource;
	toReturn.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	toReturn.Transition.StateBefore = textureEntry.currentState;
	toReturn.Transition.StateAfter = newState;

	textureEntry.currentState = newState;

	return toReturn;
}