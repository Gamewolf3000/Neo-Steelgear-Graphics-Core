#include "Texture2DComponentData.h"

void Texture2DComponentData::UpdateExistingSubresourceHeaders(
	size_t indexOfOriginalChange, std::int16_t entryDifference)
{
	for (size_t i = indexOfOriginalChange + 1; i < headers.size(); ++i)
		headers[i].specifics.startSubresource += entryDifference;

	size_t originalEntryEnd =
		headers[indexOfOriginalChange].specifics.startSubresource +
		headers[indexOfOriginalChange].specifics.nrOfSubresources;
	size_t totalEntriesToMove = subresourceHeaders.size() - originalEntryEnd;

	if(entryDifference > 0)
		subresourceHeaders.resize(subresourceHeaders.size() + entryDifference);

	SubresourceHeader* destination = subresourceHeaders.data();
	destination += headers[indexOfOriginalChange].specifics.startSubresource;
	destination += headers[indexOfOriginalChange].specifics.nrOfSubresources;
	SubresourceHeader* source = destination;
	destination += entryDifference;

	std::memmove(destination, source, totalEntriesToMove * sizeof(SubresourceHeader));
}

void Texture2DComponentData::SetSubresourceHeaders(size_t firstIndex,
	std::uint8_t nrOfSubresources, ID3D12Resource* resource)
{
	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	size_t currentOffset = 0;
	for (std::uint8_t i = 0; i < nrOfSubresources; ++i)
	{
		UINT nrOfRows = 0;
		UINT64 rowSize = 0;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		device->GetCopyableFootprints(&desc, i, 1, 0, &footprint, &nrOfRows,
			&rowSize, nullptr);

		subresourceHeaders[firstIndex + i].framesLeft = 0;
		subresourceHeaders[firstIndex + i].startOffset = currentOffset;
		subresourceHeaders[firstIndex + i].width = footprint.Footprint.Width;
		subresourceHeaders[firstIndex + i].height = footprint.Footprint.Height;
		currentOffset += static_cast<size_t>(nrOfRows * rowSize);
	}
}

std::uint8_t Texture2DComponentData::CalculateSubresourceCount(
	ID3D12Resource* resource)
{
	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = { desc.Format, 0 };
	std::uint8_t planeCount = 1;

	HRESULT hr = device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,
		&formatInfo, sizeof(formatInfo));
	if (SUCCEEDED(hr))
		planeCount = formatInfo.PlaneCount;

	std::uint8_t arrayCount =
		desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D &&
		desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE1D ? 1 :
		static_cast<std::uint8_t>(desc.DepthOrArraySize);

	return static_cast<std::uint8_t>(planeCount * arrayCount * desc.MipLevels);
}

void Texture2DComponentData::Initialize(ID3D12Device* deviceToUse, 
	FrameType totalNrOfFrames, UpdateType componentUpdateType,
	unsigned int initialSize)
{
	// Textures cannot be placed in upload heaps
	if (componentUpdateType == UpdateType::MAP_UPDATE)
		throw std::runtime_error("Texture2D component data for map update is not possible");

	ComponentData<Texture2DSpecific>::Initialize(deviceToUse, totalNrOfFrames,
		componentUpdateType, initialSize);
}

void Texture2DComponentData::AddComponent(const ResourceIndex& resourceIndex,
	unsigned int dataSize, ID3D12Resource* resource)
{
	if (type == UpdateType::NONE)
		return;

	if (usedDataSize + dataSize > data.capacity())
	{
		data.reserve(usedDataSize + dataSize);
		data.resize(usedDataSize + dataSize); // Resize so new data fits
	}

	std::uint8_t nrOfSubresources = CalculateSubresourceCount(resource);
	if (type != UpdateType::INITIALISE_ONLY && resourceIndex.descriptorIndex < headers.size())
	{
		std::int64_t sizeDifference = dataSize - headers[resourceIndex.descriptorIndex].dataSize;
		UpdateExistingHeaders(resourceIndex.descriptorIndex, sizeDifference);
		headers[resourceIndex.descriptorIndex].dataSize = dataSize;

		std::int16_t entryDifference = static_cast<std::uint16_t>(
			nrOfSubresources - headers[resourceIndex.descriptorIndex].specifics.nrOfSubresources);
		UpdateExistingSubresourceHeaders(resourceIndex.descriptorIndex, entryDifference);
		headers[resourceIndex.descriptorIndex].specifics.nrOfSubresources = nrOfSubresources;
		headers[resourceIndex.descriptorIndex].specifics.needUpdating = false;
		SetSubresourceHeaders(headers[resourceIndex.descriptorIndex].specifics.startSubresource,
			nrOfSubresources, resource);
	}
	else
	{
		DataHeader toAdd;
		toAdd.startOffset = headers.size() == 0 ? 0 :
			headers.back().startOffset + headers.back().dataSize;
		toAdd.dataSize = dataSize;
		toAdd.resourceIndex = resourceIndex;
		toAdd.specifics.startSubresource = subresourceHeaders.size();
		toAdd.specifics.nrOfSubresources = nrOfSubresources;
		toAdd.specifics.needUpdating = false;
		subresourceHeaders.resize(subresourceHeaders.size() + nrOfSubresources);
		headers.push_back(toAdd);
		SetSubresourceHeaders(toAdd.specifics.startSubresource, nrOfSubresources,
			resource);
	}

	usedDataSize += dataSize;
}

void Texture2DComponentData::RemoveComponent(const ResourceIndex& resourceIndex)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		std::int16_t subresourceDifference = 
			headers[resourceIndex.descriptorIndex].specifics.nrOfSubresources;
		subresourceDifference = -subresourceDifference;
		std::int64_t dataDifference = headers[resourceIndex.descriptorIndex].dataSize;
		dataDifference = -dataDifference;
		UpdateExistingSubresourceHeaders(resourceIndex.descriptorIndex, subresourceDifference);
		UpdateExistingHeaders(resourceIndex.descriptorIndex, dataDifference);
		headers[resourceIndex.descriptorIndex].specifics.nrOfSubresources = 0;
		headers[resourceIndex.descriptorIndex].dataSize = 0;
		usedDataSize = dataDifference >= 0 ?
			usedDataSize + static_cast<size_t>(dataDifference) :
			usedDataSize - static_cast<size_t>(-dataDifference);
	}
	else
	{
		for (size_t i = 0; i < headers.size(); ++i)
		{
			if (headers[i].resourceIndex.descriptorIndex != resourceIndex.descriptorIndex)
				continue;

			std::int16_t subresourceDifference = 
				headers[i].specifics.nrOfSubresources;
			subresourceDifference = -subresourceDifference;
			std::int64_t dataDifference = headers[i].dataSize;
			dataDifference = -dataDifference;
			UpdateExistingSubresourceHeaders(i, subresourceDifference);
			UpdateExistingHeaders(i, dataDifference);
			usedDataSize = dataDifference >= 0 ?
				usedDataSize + static_cast<size_t>(dataDifference) :
				usedDataSize - static_cast<size_t>(-dataDifference);

			for (size_t removalCounter = 0;
				removalCounter < headers[i].specifics.nrOfSubresources;
				++removalCounter)
			{
				subresourceHeaders.pop_back();
			}

			size_t moveSize = sizeof(DataHeader) * (headers.size() - i - 1);
			std::memmove(headers.data() + i, headers.data() + i + 1, moveSize);
			headers.pop_back();
			return;
		}
	}
}

void Texture2DComponentData::UpdateComponentData(const ResourceIndex& resourceIndex,
	void* dataPtr, std::uint8_t texelSizeInBytes, std::uint8_t subresource)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		size_t subresourceSlot = headers[resourceIndex.descriptorIndex].specifics.startSubresource;
		subresourceSlot += subresource;
		unsigned char* destination = data.data();
		destination += headers[resourceIndex.descriptorIndex].startOffset;
		destination += subresourceHeaders[subresourceSlot].startOffset;
		size_t dataSize = subresourceHeaders[subresourceSlot].width *
			subresourceHeaders[subresourceSlot].height * texelSizeInBytes;

		std::memcpy(destination, dataPtr, dataSize);
		subresourceHeaders[subresourceSlot].framesLeft = nrOfFrames;
		headers[resourceIndex.descriptorIndex].specifics.needUpdating = true;
		updateNeeded = true;
	}
	else
	{
		for (auto& header : headers)
		{
			if (header.resourceIndex.descriptorIndex != resourceIndex.descriptorIndex)
				continue;

			size_t subresourceSlot = header.specifics.startSubresource;
			subresourceSlot += subresource;
			unsigned char* destination = data.data();
			destination += header.startOffset;
			destination += subresourceHeaders[subresourceSlot].startOffset;
			size_t dataSize = subresourceHeaders[subresourceSlot].width *
				subresourceHeaders[subresourceSlot].height * texelSizeInBytes;

			std::memcpy(destination, dataPtr, dataSize);
			subresourceHeaders[subresourceSlot].framesLeft = nrOfFrames;
			header.specifics.needUpdating = true;
			updateNeeded = true;
			break;
		}
	}
}

void Texture2DComponentData::PrepareUpdates(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers,
	Texture2DComponent& componentToUpdate)
{
	if (this->updateNeeded == false || type == UpdateType::MAP_UPDATE)
		return;

	for (auto& header : headers)
	{
		if (header.specifics.needUpdating)
		{
			D3D12_RESOURCE_BARRIER barrier =
				componentToUpdate.CreateTransitionBarrier(header.resourceIndex,
					D3D12_RESOURCE_STATE_COPY_DEST);

			if (barrier.Transition.StateBefore != barrier.Transition.StateAfter)
				barriers.push_back(barrier);
		}
	}
}

void Texture2DComponentData::UpdateComponentResources(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader,
	Texture2DComponent& componentToUpdate, std::uint8_t texelSize,
	DXGI_FORMAT textureFormat)
{
	if (updateNeeded == false)
		return;

	updateNeeded = false;

	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (headers[i].specifics.needUpdating == false)
			continue;

		headers[i].specifics.needUpdating = false;
		auto handle = componentToUpdate.GetTextureHandle(headers[i].resourceIndex);

		for (unsigned int j = 0; j < headers[i].specifics.nrOfSubresources; ++j)
		{
			size_t subresourceIndex = headers[i].specifics.startSubresource + j;
			SubresourceHeader& subresource = subresourceHeaders[subresourceIndex];

			if (subresource.framesLeft == 0)
			{
				continue;
			}
			else if (subresource.framesLeft > 1)
			{
				updateNeeded = true; // At least one update left for next frame
				headers[i].specifics.needUpdating = true;
			}

			unsigned char* source = data.data();
			source += headers[i].startOffset + subresource.startOffset;

			TextureUploadInfo uploadInfo;
			uploadInfo.texelSizeInBytes = texelSize;
			uploadInfo.format = textureFormat;
			uploadInfo.width = subresource.width;
			uploadInfo.height = subresource.height;
			uploadInfo.depth = 1;
			bool result = uploader.UploadTextureResourceData(handle.resource, commandList,
				source, uploadInfo, j);

			if (result == false && type == UpdateType::COPY_UPDATE)
			{
				updateNeeded = true;
				headers[i].specifics.needUpdating = true;
				throw std::runtime_error("Could not update data for texture2D component");
			}
			else if (result == false && type == UpdateType::INITIALISE_ONLY)
			{
				updateNeeded = true; // At least one update left for next frame
				headers[i].specifics.needUpdating = true;
				subresource.framesLeft += nrOfFrames;
			}

			--subresource.framesLeft;
		}

		// If INITIALISE_ONLY is used and all frames are updated then we are finished with this one
		if (type == UpdateType::INITIALISE_ONLY && !headers[i].specifics.needUpdating)
		{
			RemoveComponent(headers[i].resourceIndex);
			--i;
		}
	}
}

