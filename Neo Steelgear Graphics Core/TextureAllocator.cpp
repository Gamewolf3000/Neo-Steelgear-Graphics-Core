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
	toReturn.Format = textureInfo.format;
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
	textureInfo(other.textureInfo), mappedTextures(other.mappedTextures),
	textures(std::move(other.textures))
{
	other.device = nullptr;
	other.textureInfo = TextureInfo();
	other.mappedTextures = false;
}

TextureAllocator& TextureAllocator::operator=(TextureAllocator&& other) noexcept
{
	if (this != &other)
	{
		ResourceAllocator::operator=(std::move(other));
		device = other.device;
		other.device = nullptr;
		textureInfo = other.textureInfo;
		other.textureInfo = TextureInfo();
		mappedTextures = other.mappedTextures;
		other.mappedTextures = false;
		textures = std::move(other.textures);
	}

	return *this;
}

void TextureAllocator::Initialize(const TextureInfo& textureInfoToUse, 
	ID3D12Device* deviceToUse, bool mappedUpdateable, 
	const AllowedViews& allowedViews, ID3D12Heap* heap, size_t startOffset,
	size_t endOffset)
{
	ResourceAllocator::Initialize(allowedViews);
	device = deviceToUse;
	mappedTextures = mappedUpdateable;
	textureInfo = textureInfoToUse;
	textures.Initialize(endOffset - startOffset);
	heapData.heapOwned = false;
	heapData.heap = heap;
	heapData.startOffset = startOffset;
	heapData.endOffset = endOffset;
}

void TextureAllocator::Initialize(const TextureInfo& textureInfoToUse,
	ID3D12Device* deviceToUse, bool mappedUpdateable,
	const AllowedViews& allowedViews, size_t heapSize)
{
	ResourceAllocator::Initialize(allowedViews);
	device = deviceToUse;
	mappedTextures = mappedUpdateable;
	textureInfo = textureInfoToUse;
	textures.Initialize(heapSize);
	heapData.heapOwned = true;
	D3D12_HEAP_FLAGS heapFlag = allowedViews.dsv || allowedViews.rtv ?
		D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES :
		D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	heapData.heap = AllocateHeap(heapSize, mappedUpdateable, heapFlag, device);
	heapData.startOffset = 0;
	heapData.endOffset = heapSize;
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

size_t TextureAllocator::GetTexelSize()
{
	return textureInfo.texelSize;
}

DXGI_FORMAT TextureAllocator::GetTextureFormat()
{
	return textureInfo.format;
}

void TextureAllocator::UpdateMappedTexture(size_t index, void* data,
	unsigned int subresource)
{
	ID3D12Resource* toUploadTo = textures[index].resource;
	D3D12_RESOURCE_DESC resourceDesc = toUploadTo->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT numRows = 0;
	UINT64 rowSizeInBytes = 0;
	UINT64 totalBytes = 0;
	device->GetCopyableFootprints(&resourceDesc, subresource, 1, 0, &footprint,
		&numRows, &rowSizeInBytes, &totalBytes);

	unsigned char* mappedPtr = nullptr;
	D3D12_RANGE nothing = { 0, 0 };
	toUploadTo->Map(subresource, &nothing, reinterpret_cast<void**>(&mappedPtr));

	std::uint64_t sourceOffset = 0;
	std::uint64_t destinationOffset = 0;
	for (UINT row = 0; row < numRows; ++row)
	{
		std::memcpy(mappedPtr + destinationOffset,
			static_cast<unsigned char*>(data) + sourceOffset, 
			static_cast<size_t>(rowSizeInBytes));
		sourceOffset += rowSizeInBytes;
		destinationOffset += footprint.Footprint.RowPitch;
	}
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

//TextureAllocationInfo::TextureAllocationInfo(size_t width, size_t arraySize, 
//	size_t mipLevels, std::uint8_t sampleCount, std::uint8_t sampleQuality,
//	D3D12_CLEAR_VALUE* clearValue) : dimensions({ width, 0, arraySize }),
//	textureType(D3D12_RESOURCE_DIMENSION_TEXTURE1D), mipLevels(mipLevels),
//	sampleCount(sampleCount), sampleQuality(sampleQuality), clearValue(clearValue)
//{
//	// Empty
//}

TextureAllocationInfo::TextureAllocationInfo(size_t width, size_t height, 
	size_t arraySize, size_t mipLevels, std::uint8_t sampleCount,
	std::uint8_t sampleQuality, D3D12_CLEAR_VALUE* clearValue) :
	dimensions({ width, height, arraySize }),
	textureType(D3D12_RESOURCE_DIMENSION_TEXTURE2D), mipLevels(mipLevels),
	sampleCount(sampleCount), sampleQuality(sampleQuality)
{
	if (clearValue == nullptr)
		this->clearValue = std::nullopt;
	else
		this->clearValue = *clearValue;
}
//
//TextureAllocationInfo::TextureAllocationInfo(size_t width, size_t height, size_t depth,
//	size_t mipLevels, std::uint8_t sampleCount, std::uint8_t sampleQuality,
//	D3D12_CLEAR_VALUE* clearValue) : dimensions({ width, height, depth }),
//	textureType(D3D12_RESOURCE_DIMENSION_TEXTURE3D), mipLevels(mipLevels),
//	sampleCount(sampleCount), sampleQuality(sampleQuality), clearValue(clearValue)
//{
//	// Empty
//}