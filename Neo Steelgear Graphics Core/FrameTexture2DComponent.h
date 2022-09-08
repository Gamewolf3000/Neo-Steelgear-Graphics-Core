#pragma once

#include <d3d12.h>
#include <optional>

#include "FrameResourceComponent.h"
#include "Texture2DComponent.h"
#include "Texture2DComponentData.h"

struct Texture2DCreationOperation
{
	size_t width = 0;
	size_t height = 0;
	size_t arraySize = 0;
	size_t mipLevels = 0;
	std::uint8_t sampleCount = 0;
	std::uint8_t sampleQuality = 0;
	std::optional<D3D12_CLEAR_VALUE> clearValue = std::nullopt;
	Texture2DComponentTemplate::TextureReplacementViews replacementViews;
};

template<FrameType Frames>
class FrameTexture2DComponent : public FrameResourceComponent<
	Texture2DComponent, Frames, Texture2DCreationOperation>
{
private:
	ID3D12Device* device = nullptr;

	typedef typename FrameResourceComponent<Texture2DComponent, Frames,
		Texture2DCreationOperation>::LifetimeOperationType Texture2DLifetimeOperationType;

	std::uint8_t texelSize = 0;
	DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
	Texture2DComponentData componentData;

	void HandleStoredOperations() override;

public:
	FrameTexture2DComponent() = default;
	FrameTexture2DComponent(const FrameTexture2DComponent& other) = delete;
	FrameTexture2DComponent& operator=(const FrameTexture2DComponent& other) = delete;
	FrameTexture2DComponent(FrameTexture2DComponent&& other) noexcept;
	FrameTexture2DComponent& operator=(FrameTexture2DComponent&& other) noexcept;

	void Initialize(ID3D12Device* deviceToUse, UpdateType componentUpdateType,
		const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
		descriptorInfo);

	ResourceIndex CreateTexture(size_t width, size_t height, size_t arraySize = 1,
		size_t mipLevels = 1, std::uint8_t sampleCount = 1,
		std::uint8_t sampleQuality = 0, D3D12_CLEAR_VALUE* clearValue = nullptr,
		const Texture2DComponentTemplate::TextureReplacementViews& replacementViews = {});

	void RemoveComponent(const ResourceIndex& indexToRemove) override;

	void SetUpdateData(const ResourceIndex& resourceIndex, void* dataAdress,
		std::uint8_t subresource);
	void PrepareResourcesForUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers);
	void PerformUpdates(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader);

	D3D12_RESOURCE_STATES GetCurrentState(const ResourceIndex& resourceIndex);
	void ChangeToState(const ResourceIndex& resourceIndex,
		std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_STATES newState,
		std::optional<D3D12_RESOURCE_STATES> assumedInitialState = std::nullopt);
	void TransitionAllTextures(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_STATES newState,
		std::optional<D3D12_RESOURCE_STATES> assumedInitialState = std::nullopt);

	TextureHandle GetTextureHandle(const ResourceIndex& index);
	const TextureHandle GetTextureHandle(const ResourceIndex& index) const;
};

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::HandleStoredOperations()
{
	size_t nrToErase = 0;
	for (auto& operation : this->storedLifetimeOperations)
	{
		if (operation.type == Texture2DLifetimeOperationType::CREATION)
		{
			Texture2DCreationOperation creationInfo = operation.creation;
			auto identifier = this->resourceComponents[this->activeFrame].CreateTexture(
				creationInfo.width, creationInfo.height, 
				creationInfo.arraySize, creationInfo.mipLevels,
				creationInfo.sampleCount, creationInfo.sampleQuality,
				creationInfo.clearValue.has_value() ? &(*creationInfo.clearValue) : nullptr,
				creationInfo.replacementViews);

			TextureHandle handle =
				this->resourceComponents[this->activeFrame].GetTextureHandle(identifier);
			this->AddInitializationBarrier(handle.resource);
		}
		else
		{
			this->resourceComponents[this->activeFrame].RemoveComponent(
				operation.removal.indexToRemove);
		}

		--operation.framesLeft;
		nrToErase += operation.framesLeft == 0 ? 1 : 0;
	}

	this->storedLifetimeOperations.erase(this->storedLifetimeOperations.begin(),
		this->storedLifetimeOperations.begin() + nrToErase);
}

template<FrameType Frames>
inline FrameTexture2DComponent<Frames>::FrameTexture2DComponent(
	FrameTexture2DComponent&& other) noexcept : FrameResourceComponent<
	Texture2DComponent, Frames, Texture2DCreationOperation>(std::move(other)),
	device(other.device), texelSize(other.texelSize), 
	textureFormat(other.textureFormat), componentData(std::move(other.componentData))
{
	other.device = nullptr;
	other.texelSize = 0;
	other.textureFormat = DXGI_FORMAT_UNKNOWN;
}

template<FrameType Frames>
inline FrameTexture2DComponent<Frames>& 
FrameTexture2DComponent<Frames>::operator=(FrameTexture2DComponent&& other) noexcept
{
	if (this != &other)
	{
		FrameResourceComponent<Texture2DComponent,
			Frames, Texture2DCreationOperation>::operator=(std::move(other));
		device = other.device;
		texelSize = other.texelSize;
		textureFormat = other.textureFormat;
		componentData = std::move(other.componentData);

		other.device = nullptr;
		other.texelSize = 0;
		other.textureFormat = DXGI_FORMAT_UNKNOWN;
	}

	return *this;
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::Initialize(ID3D12Device* deviceToUse,
	UpdateType componentUpdateType, const TextureComponentInfo& textureInfo,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
	descriptorInfo)
{
	FrameResourceComponent<Texture2DComponent, Frames,
		Texture2DCreationOperation>::Initialize(deviceToUse, textureInfo,
			descriptorInfo);
	device = deviceToUse;
	texelSize = textureInfo.texelSize;
	textureFormat = textureInfo.format;
	if (componentUpdateType != UpdateType::INITIALISE_ONLY &&
		componentUpdateType != UpdateType::NONE)
	{
		unsigned int initialSize = 
			static_cast<unsigned int>(textureInfo.memoryInfo.initialMinimumHeapSize);

		this->componentData.Initialize(deviceToUse, Frames, 
			componentUpdateType, initialSize);
	}
	else
	{
		this->componentData.Initialize(deviceToUse, Frames, 
			componentUpdateType, 0);
	}
}

template<FrameType Frames>
inline ResourceIndex FrameTexture2DComponent<Frames>::CreateTexture(
	size_t width, size_t height, size_t arraySize, size_t mipLevels,
	std::uint8_t sampleCount, std::uint8_t sampleQuality,
	D3D12_CLEAR_VALUE* clearValue,
	const TextureComponent<Texture2DShaderResourceDesc,
	Texture2DUnorderedAccessDesc, Texture2DRenderTargetDesc,
	Texture2DDepthStencilDesc>::TextureReplacementViews& replacementViews)
{
	ResourceIndex toReturn =
		this->resourceComponents[this->activeFrame].CreateTexture(
			width, height, arraySize, mipLevels, sampleCount, 
			sampleQuality, clearValue, replacementViews);

	if constexpr (Frames != 1)
	{
		typename FrameResourceComponent<Texture2DComponent, Frames,
			Texture2DCreationOperation>::StoredLifetimeOperation lifetimeOperation;
		lifetimeOperation.type = Texture2DLifetimeOperationType::CREATION;
		lifetimeOperation.framesLeft = Frames - 1;
		lifetimeOperation.creation.width = width; 
		lifetimeOperation.creation.height = height;
		lifetimeOperation.creation.arraySize = arraySize;
		lifetimeOperation.creation.mipLevels = mipLevels;
		lifetimeOperation.creation.sampleCount = sampleCount;
		lifetimeOperation.creation.sampleQuality = sampleQuality;
		lifetimeOperation.creation.clearValue =
			(clearValue == nullptr ? std::nullopt :
				std::make_optional(*clearValue));
		lifetimeOperation.creation.replacementViews = replacementViews;
		this->storedLifetimeOperations.push_back(lifetimeOperation);
	}

	TextureHandle handle = 
		this->resourceComponents[this->activeFrame].GetTextureHandle(toReturn);
	this->AddInitializationBarrier(handle.resource);
	auto desc = handle.resource->GetDesc();
	unsigned int totalSize = 0;

	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = { desc.Format, 0 };
	std::uint8_t planeCount = 1;
	HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,
		&formatInfo, sizeof(formatInfo));
	if (SUCCEEDED(hr))
		planeCount = formatInfo.PlaneCount;

	UINT nrOfSubresources = planeCount * desc.DepthOrArraySize * desc.MipLevels;
	std::vector<UINT> rows(nrOfSubresources);
	std::vector<UINT64> rowSizes(nrOfSubresources);

	device->GetCopyableFootprints(&desc, 0, nrOfSubresources, 0, nullptr,
		rows.data(), rowSizes.data(), nullptr);

	for (unsigned int subresource = 0; subresource < nrOfSubresources; ++subresource)
	{
		totalSize += static_cast<unsigned int>(rows[subresource] *
			rowSizes[subresource]);
	}

	this->componentData.AddComponent(toReturn, totalSize, handle.resource);

	return toReturn;
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::RemoveComponent(
	const ResourceIndex& indexToRemove)
{
	FrameResourceComponent<Texture2DComponent, Frames,
		Texture2DCreationOperation>::RemoveComponent(indexToRemove);
	componentData.RemoveComponent(indexToRemove);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::SetUpdateData(
	const ResourceIndex& resourceIndex, void* dataAdress, std::uint8_t subresource)
{
	this->componentData.UpdateComponentData(resourceIndex, dataAdress,
		texelSize, subresource);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::PrepareResourcesForUpdates(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers)
{
	this->componentData.PrepareUpdates(barriers,
		this->resourceComponents[this->activeFrame]);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::PerformUpdates(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader)
{
	this->componentData.UpdateComponentResources(commandList, uploader,
		this->resourceComponents[this->activeFrame], texelSize, textureFormat);
}

template<FrameType Frames>
inline D3D12_RESOURCE_STATES FrameTexture2DComponent<Frames>::GetCurrentState(
	const ResourceIndex& resourceIndex)
{
	return this->resourceComponents[this->activeFrame].GetCurrentState(
		resourceIndex);
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::ChangeToState(
	const ResourceIndex& resourceIndex,
	std::vector<D3D12_RESOURCE_BARRIER>& barriers, D3D12_RESOURCE_STATES newState,
	std::optional<D3D12_RESOURCE_STATES> assumedInitialState)
{
	barriers.push_back(
		this->resourceComponents[this->activeFrame].CreateTransitionBarrier(
			resourceIndex, newState, D3D12_RESOURCE_BARRIER_FLAG_NONE, assumedInitialState));
}

template<FrameType Frames>
inline void FrameTexture2DComponent<Frames>::TransitionAllTextures(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers, D3D12_RESOURCE_STATES newState,
	std::optional<D3D12_RESOURCE_STATES> assumedInitialState)
{
	this->resourceComponents[this->activeFrame].TransitionAllTextures(
		barriers, newState, D3D12_RESOURCE_BARRIER_FLAG_NONE, assumedInitialState);
}

template<FrameType Frames>
inline TextureHandle FrameTexture2DComponent<Frames>::GetTextureHandle(
	const ResourceIndex& index)
{
	return this->resourceComponents[this->activeFrame].GetTextureHandle(index);
}

template<FrameType Frames>
inline const TextureHandle FrameTexture2DComponent<Frames>::GetTextureHandle(const ResourceIndex& index) const
{
	return this->resourceComponents[this->activeFrame].GetTextureHandle(index);
}
