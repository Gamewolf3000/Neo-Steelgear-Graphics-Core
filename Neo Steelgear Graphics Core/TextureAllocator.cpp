#include "TextureAllocator.h"

D3D12_RESOURCE_DESC TextureAllocator::CreateTextureDesc(
	const TextureAllocationInfo& info)
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
	toReturn.Flags = CreateBindFlag();

	return toReturn;
}

TextureAllocator::~TextureAllocator()
{
	textures.ClearHeap();
}

TextureAllocator::TextureAllocator(TextureAllocator&& other) noexcept : 
	ResourceAllocator(std::move(other)), device(other.device),
	textures(std::move(other.textures))
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
		textures = std::move(other.textures);
	}

	return *this;
}

void TextureAllocator::Initialize(ID3D12Device* deviceToUse,
	const AllowedViews& allowedViews, ID3D12Heap* heap,
	size_t startOffset, size_t endOffset)
{
	ResourceAllocator::Initialize(allowedViews);
	device = deviceToUse;
	textures.Initialize(endOffset - startOffset);
	heapData.heapOwned = false;
	heapData.heap = heap;
	heapData.startOffset = startOffset;
	heapData.endOffset = endOffset;
}

void TextureAllocator::Initialize(ID3D12Device* deviceToUse,
	const AllowedViews& allowedViews, size_t heapSize)
{
	ResourceAllocator::Initialize(allowedViews);
	device = deviceToUse;
	textures.Initialize(heapSize);
	heapData.heapOwned = true;
	D3D12_HEAP_FLAGS heapFlag = allowedViews.dsv || allowedViews.rtv ?
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES :
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	heapData.heap = AllocateHeap(heapSize, false, heapFlag, device);
	heapData.startOffset = 0;
	heapData.endOffset = heapSize;
}

void TextureAllocator::ResizeAllocator(size_t newSize, 
	ID3D12GraphicsCommandList* list)
{
	if (heapData.heapOwned == false)
		throw std::runtime_error("Illegal attempt to resize non owned heap");

	D3D12_HEAP_FLAGS heapFlag = views.dsv || views.rtv ?
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES :
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	auto oldHeap = heapData.heap;
	heapData.heap = AllocateHeap(newSize, false, heapFlag, device);
	heapData.endOffset = newSize;

	textures.AddChunk(newSize - textures.TotalSize(), true);

	for (unsigned int i = 0; i < textures.GetCurrentMaxIndex(); ++i)
	{
		if (textures.ChunkActive(i))
		{
			auto desc = textures[i].resource->GetDesc();
			const D3D12_CLEAR_VALUE* clearValue = textures[i].clearValue.has_value() ?
				&textures[i].clearValue.value() : nullptr;

			ID3D12Resource* oldResource = textures[i].resource;
			textures[i].resource = ResourceAllocator::AllocateResource(desc,
				textures[i].currentState, clearValue, textures.GetStartOfChunk(i), device);

			if (list != nullptr)
			{
				list->CopyResource(textures[i].resource, oldResource);
				oldResources.push_back(oldResource);
			}
			else
			{
				oldResource->Release();
			}
		}
	}

	if (list != nullptr)
		oldHeaps.push_back(oldHeap);
	else
		oldHeap->Release();
}

void TextureAllocator::ResetAllocator()
{
	ResourceAllocator::ClearOldResources();
	textures.ClearHeap();
}

size_t TextureAllocator::AllocateTexture(const TextureAllocationInfo& info)
{
	D3D12_RESOURCE_DESC desc = CreateTextureDesc(info);
	auto resourceInfo = device->GetResourceAllocationInfo(0, 1, &desc);

	size_t textureEntryIndex = textures.AllocateChunk(
		static_cast<size_t>(resourceInfo.SizeInBytes),
		AllocationStrategy::FIRST_FIT, static_cast<size_t>(resourceInfo.Alignment));

	if (textureEntryIndex != size_t(-1))
	{
		const D3D12_CLEAR_VALUE* clearValue = info.clearValue.has_value() ?
			&info.clearValue.value() : nullptr;
		
		textures[textureEntryIndex].resource = 
			ResourceAllocator::AllocateResource(desc, D3D12_RESOURCE_STATE_COMMON,
				clearValue, textures.GetStartOfChunk(textureEntryIndex), device);
		textures[textureEntryIndex].currentState = D3D12_RESOURCE_STATE_COMMON;
		textures[textureEntryIndex].dimensions = info.dimensions;
		textures[textureEntryIndex].texelSize = info.texelSize;
		textures[textureEntryIndex].clearValue = clearValue != nullptr ?
			std::make_optional(*clearValue) : std::nullopt;
	}

	return textureEntryIndex;
}

void TextureAllocator::DeallocateTexture(size_t index)
{
	textures[index].resource->Release();
	textures[index].resource = nullptr;
	textures.DeallocateChunk(index);
}

TextureHandle TextureAllocator::GetHandle(size_t index)
{
	TextureHandle toReturn;
	toReturn.resource = textures[index].resource;
	toReturn.dimensions = textures[index].dimensions;

	return toReturn;
}

D3D12_RESOURCE_STATES TextureAllocator::GetCurrentState(size_t index)
{
	return textures[index].currentState;
}

D3D12_RESOURCE_BARRIER TextureAllocator::CreateTransitionBarrier(size_t index,
	D3D12_RESOURCE_STATES newState, D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	D3D12_RESOURCE_BARRIER toReturn;

	toReturn.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toReturn.Flags = flag;
	toReturn.Transition.pResource = textures[index].resource;
	toReturn.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	toReturn.Transition.StateBefore = textures[index].currentState;
	toReturn.Transition.StateAfter = newState;

	textures[index].currentState = newState;

	return toReturn;
}