#pragma once

#include <d3d12.h>
#include <dxgi.h>
#include <vector>
#include <utility>
#include <optional>

#include "ResourceAllocator.h"
#include "HeapHelper.h"
#include "ResourceUploader.h"

struct TextureDimensions
{
	size_t width = 0;
	size_t height = 0;
	size_t depthOrArraySize = 0;
};

struct TextureAllocationInfo
{
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	std::uint8_t texelSize = 0;
	TextureDimensions dimensions;
	D3D12_RESOURCE_DIMENSION textureType;
	size_t mipLevels;
	std::uint8_t sampleCount;
	std::uint8_t sampleQuality;
	std::optional<D3D12_CLEAR_VALUE> clearValue;

	TextureAllocationInfo(DXGI_FORMAT format, std::uint8_t texelSize,
		size_t width, size_t height, size_t arraySize = 1,
		size_t mipLevels = 1, std::uint8_t sampleCount = 1,
		std::uint8_t sampleQuality = 0, D3D12_CLEAR_VALUE* clearValue = nullptr) :
		format(format), texelSize(texelSize), dimensions({ width, height, arraySize }),
		textureType(D3D12_RESOURCE_DIMENSION_TEXTURE2D), mipLevels(mipLevels),
		sampleCount(sampleCount), sampleQuality(sampleQuality)
	{
		if (clearValue == nullptr)
			this->clearValue = std::nullopt;
		else
			this->clearValue = *clearValue;
	}
};

struct TextureHandle
{
	ID3D12Resource* resource = nullptr;
	TextureDimensions dimensions;
};

class TextureAllocator : public ResourceAllocator
{
private:

	struct TextureEntry
	{
		ID3D12Resource* resource = nullptr;
		D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
		TextureDimensions dimensions;
		std::uint8_t texelSize = 0;
		std::optional<D3D12_CLEAR_VALUE> clearValue = std::nullopt;

		TextureEntry()
		{
			resource = nullptr;
			currentState = D3D12_RESOURCE_STATE_COMMON;
			texelSize = 0;
		}

		TextureEntry(const TextureEntry& other) = delete;
		TextureEntry& operator=(const TextureEntry& other) = delete;

		TextureEntry(TextureEntry&& other) noexcept : resource(other.resource),
			currentState(other.currentState), dimensions(other.dimensions),
			texelSize(other.texelSize), clearValue(std::move(other.clearValue))
		{
			other.resource = nullptr;
			other.currentState = D3D12_RESOURCE_STATE_COMMON;
			other.dimensions = TextureDimensions();
			other.texelSize = 0;
		}

		TextureEntry& operator=(TextureEntry&& other) noexcept
		{
			if (this != &other)
			{
				resource = other.resource;
				other.resource = nullptr;
				currentState = other.currentState;
				other.currentState = D3D12_RESOURCE_STATE_COMMON;
				dimensions = other.dimensions;
				other.dimensions = TextureDimensions();
				texelSize = other.texelSize;
				other.texelSize = 0;
				clearValue = std::move(other.clearValue);
			}

			return *this;
		}

		~TextureEntry()
		{
			if (resource != nullptr)
				resource->Release();
		}
	};

	ID3D12Device* device = nullptr;
	HeapHelper<TextureEntry> textures;

	D3D12_RESOURCE_DESC CreateTextureDesc(const TextureAllocationInfo& info,
		std::optional<D3D12_RESOURCE_FLAGS> replacementBindings);

public:
	TextureAllocator() = default;
	~TextureAllocator();
	TextureAllocator(const TextureAllocator& other) = delete;
	TextureAllocator& operator=(const TextureAllocator& other) = delete;
	TextureAllocator(TextureAllocator&& other) noexcept;
	TextureAllocator& operator=(TextureAllocator&& other) noexcept;

	void Initialize(ID3D12Device* deviceToUse,
		const AllowedViews& allowedViews, ID3D12Heap* heap,
		size_t startOffset, size_t endOffset);
	void Initialize(ID3D12Device* deviceToUse,
		const AllowedViews& allowedViews, size_t heapSize);

	void ResizeAllocator(size_t newSize, ID3D12GraphicsCommandList* list = nullptr);
	void ResetAllocator();

	size_t AllocateTexture(const TextureAllocationInfo& info, 
		std::optional<D3D12_RESOURCE_FLAGS> replacementBindings = std::nullopt);

	void DeallocateTexture(size_t index);

	D3D12_RESOURCE_BARRIER CreateTransitionBarrier(size_t index,
		D3D12_RESOURCE_STATES newState,
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE);

	TextureHandle GetHandle(size_t index);
	D3D12_RESOURCE_STATES GetCurrentState(size_t index);
};