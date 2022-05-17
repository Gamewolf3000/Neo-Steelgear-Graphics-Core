#include "TextureComponentData.h"

void Texture2DComponentData::UpdateExistingSubresourceHeaders(
	size_t indexOfOriginalChange, std::int16_t entryDifference)
{
	for (size_t i = indexOfOriginalChange + 1; i < headers.size(); ++i)
		headers[i].specifics.startSubresource += entryDifference;

	//SubresourceHeader* destination = subresourceHeaders.data();
	//destination += headers[indexOfOriginalChange].specifics.nrOfSubresources;
	//SubresourceHeader* source = destination - entryDifference;
	//size_t originalEntryEnd =
	//	headers[indexOfOriginalChange].specifics.startSubresource +
	//	headers[indexOfOriginalChange].specifics.nrOfSubresources;
	//size_t totalEntriesToMove = subresourceHeaders.size() - originalEntryEnd;

	//if (subresourceHeaders.size() + entryDifference > subresourceHeaders.capacity())
	//	subresourceHeaders.resize(subresourceHeaders.size() + entryDifference);

	//std::memmove(destination, source, totalEntriesToMove);

	SubresourceHeader* destination = subresourceHeaders.data();
	destination += headers[indexOfOriginalChange].specifics.startSubresource;
	destination += headers[indexOfOriginalChange].specifics.nrOfSubresources;
	SubresourceHeader* source = destination;
	destination += entryDifference;
	size_t originalEntryEnd =
		headers[indexOfOriginalChange].specifics.startSubresource +
		headers[indexOfOriginalChange].specifics.nrOfSubresources;
	size_t totalEntriesToMove = subresourceHeaders.size() - originalEntryEnd;

	if (subresourceHeaders.size() + entryDifference > subresourceHeaders.capacity())
		subresourceHeaders.resize(subresourceHeaders.size() + entryDifference);

	std::memmove(destination, source, totalEntriesToMove * sizeof(SubresourceHeader));
}

void Texture2DComponentData::SetSubresourceHeaders(size_t firstIndex,
	std::uint8_t nrOfSubresources, ID3D12Resource* resource)
{
	D3D12_RESOURCE_DESC desc = resource->GetDesc();
	size_t currentOffset = 0;
	for (std::uint8_t i = 0; i < nrOfSubresources; ++i)
	{
		UINT64 totalBytes = 0;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
		device->GetCopyableFootprints(&desc, i, 1, 0, &footprint, nullptr,
			nullptr, &totalBytes);

		subresourceHeaders[firstIndex + i].framesLeft = 0;
		subresourceHeaders[firstIndex + i].startOffset = currentOffset;
		subresourceHeaders[firstIndex + i].width = footprint.Footprint.Width;
		subresourceHeaders[firstIndex + i].height = footprint.Footprint.Height;
		currentOffset += static_cast<size_t>(totalBytes);
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

void Texture2DComponentData::AddComponent(ResourceIndex resourceIndex,
	unsigned int dataSize, ID3D12Resource* resource)
{
	if (type == UpdateType::NONE)
		return;

	std::uint8_t nrOfSubresources = CalculateSubresourceCount(resource);
	if (type != UpdateType::INITIALISE_ONLY && resourceIndex < headers.size())
	{
		std::int64_t sizeDifference = dataSize - headers[resourceIndex].dataSize;
		headers[resourceIndex].dataSize = dataSize;
		UpdateExistingHeaders(resourceIndex, sizeDifference);

		std::int16_t entryDifference = static_cast<std::uint16_t>(
			nrOfSubresources - headers[resourceIndex].specifics.nrOfSubresources);
		UpdateExistingSubresourceHeaders(resourceIndex, entryDifference);
		headers[resourceIndex].specifics.nrOfSubresources = nrOfSubresources;
		headers[resourceIndex].specifics.needUpdating = false;
		SetSubresourceHeaders(headers[resourceIndex].specifics.startSubresource,
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

		if (type == UpdateType::INITIALISE_ONLY)
		{
			if (usedDataSize + dataSize > data.capacity())
			{
				data.reserve(usedDataSize + dataSize);
				data.resize(usedDataSize + dataSize); // Resize so new data fits
			}

			usedDataSize += dataSize;
		}
	}
}

void Texture2DComponentData::RemoveComponent(ResourceIndex resourceIndex)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		std::int16_t subresourceDifference = 
			headers[resourceIndex].specifics.nrOfSubresources;
		subresourceDifference = -subresourceDifference;
		std::int64_t dataDifference = headers[resourceIndex].dataSize;
		dataDifference = -dataDifference;
		UpdateExistingSubresourceHeaders(resourceIndex, subresourceDifference);
		UpdateExistingHeaders(resourceIndex, dataDifference);
		headers[resourceIndex].specifics.nrOfSubresources = 0;
		headers[resourceIndex].dataSize = 0;
	}
	else
	{
		for (size_t i = 0; i < headers.size(); ++i)
		{
			if (headers[i].resourceIndex != resourceIndex)
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

			size_t moveSize = sizeof(DataHeader) * (headers.size() - i - 1);
			std::memmove(headers.data() + i, headers.data() + i + 1, moveSize);
			headers.pop_back();
			subresourceHeaders.pop_back();
			return;
		}
	}
}

void Texture2DComponentData::UpdateComponentData(ResourceIndex resourceIndex,
	void* dataPtr, std::uint8_t texelSizeInBytes, std::uint8_t subresource)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		size_t subresourceSlot = headers[resourceIndex].specifics.startSubresource;
		subresourceSlot += subresource;
		unsigned char* destination = data.data();
		destination += headers[resourceIndex].startOffset;
		destination += subresourceHeaders[subresourceSlot].startOffset;
		size_t dataSize = subresourceHeaders[subresourceSlot].width *
			subresourceHeaders[subresourceSlot].height * texelSizeInBytes;
		std::memcpy(destination, dataPtr, dataSize);
		subresourceHeaders[subresourceSlot].framesLeft = nrOfFrames;
		headers[resourceIndex].specifics.needUpdating = true;
		updateNeeded = true;
	}
	else
	{
		for (auto& header : headers)
		{
			if (header.resourceIndex != resourceIndex)
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

void Texture2DComponentData::PrepareUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers, Texture2DComponent& componentToUpdate)
{
	// A mapped resource is in an upload heap which needs to be generic read state
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

			if (type != UpdateType::MAP_UPDATE)
			{
				TextureUploadInfo uploadInfo;
				uploadInfo.texelSizeInBytes = texelSize;
				uploadInfo.format = textureFormat;
				uploadInfo.width = subresource.width;
				uploadInfo.height = subresource.height;
				uploadInfo.depth = 1;

				uploader.UploadTextureResourceData(handle.resource, commandList,
					source, uploadInfo, 0);
			}
			else
			{
				// FIX THIS LATER, IT IS LITERALLY NOT POSSIBLE TO MAP UPDATE A TEXTURE!!!
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

