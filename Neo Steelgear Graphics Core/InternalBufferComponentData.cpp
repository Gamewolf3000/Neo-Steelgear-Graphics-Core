#include "InternalBufferComponentData.h"

void InternalBufferComponentData::HandleInitializeOnlyUpdate(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader,
	BufferComponent& componentToUpdate, size_t componentAlignment)
{
	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (headers[i].specifics.framesLeft == 0)
			continue;
		else if (headers[i].specifics.framesLeft > 1)
			updateNeeded = true; // At least one update left for next frame

		auto handle = componentToUpdate.GetBufferHandle(
			headers[i].resourceIndex);
		unsigned char* source = data.data();
		source += headers[i].startOffset;

		bool result = uploader.UploadBufferResourceData(handle.resource,
			commandList, source, handle.startOffset, headers[i].dataSize,
			componentAlignment);

		if (!result)
		{
			updateNeeded = true;
			headers[i].specifics.framesLeft += nrOfFrames;
		}

		--headers[i].specifics.framesLeft;

		// If INITIALISE_ONLY is used and all frames are updated then we are finished with this one
		if (headers[i].specifics.framesLeft == 0)
		{
			RemoveComponent(headers[i].resourceIndex);
			--i;
		}
	}
}

void InternalBufferComponentData::HandleCopyUpdate(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader,
	BufferComponent& componentToUpdate, size_t componentAlignment)
{
	size_t earliestOffset = static_cast<size_t>(-1);
	size_t latestEnd = 0;
	ID3D12Resource* resource = nullptr;

	for (auto& header : headers)
	{
		if (header.specifics.framesLeft == 0)
			continue;
		else if (header.specifics.framesLeft > 1)
			updateNeeded = true; // At least one update left for next frame

		earliestOffset = min(header.startOffset, earliestOffset);
		latestEnd = max(header.startOffset + header.dataSize, latestEnd);
		--header.specifics.framesLeft;
		resource =
			componentToUpdate.GetBufferHandle(header.resourceIndex).resource;
	}

	if (resource != nullptr)
	{
		bool result = uploader.UploadBufferResourceData(resource, commandList,
			data.data() + earliestOffset, earliestOffset,
			latestEnd - earliestOffset, componentAlignment);

		if (result == false)
			throw std::runtime_error("Could not update data for buffer component");
	}
}

void InternalBufferComponentData::HandleMapUpdate(BufferComponent& componentToUpdate)
{
	ResourceIndex earliestResourcIndex;
	size_t earliestOffset = static_cast<size_t>(-1);
	size_t latestEnd = 0;

	for (auto& header : headers)
	{
		if (header.specifics.framesLeft == 0)
			continue;
		else if (header.specifics.framesLeft > 1)
			updateNeeded = true; // At least one update left for next frame

		earliestOffset = min(header.startOffset, earliestOffset);
		earliestResourcIndex = header.startOffset == earliestOffset ?
			header.resourceIndex : earliestResourcIndex;
		latestEnd = max(header.startOffset + header.dataSize, latestEnd);
		--header.specifics.framesLeft;
	}

	if (latestEnd != 0)
	{
		unsigned char* mapped = componentToUpdate.GetMappedPtr(earliestResourcIndex);
		memcpy(mapped, data.data() + earliestOffset, latestEnd - earliestOffset);
	}
}

void InternalBufferComponentData::AddComponent(const ResourceIndex& resourceIndex,
	size_t startOffset, unsigned int dataSize, void* initialData)
{
	if (type == UpdateType::NONE)
		return;

	if (usedDataSize + dataSize > data.capacity())
	{
		data.reserve(usedDataSize + dataSize);
		data.resize(usedDataSize + dataSize); // Resize so new data fits
	}

	if (type != UpdateType::INITIALISE_ONLY)
	{
		if(headers.size() <= resourceIndex.descriptorIndex)
			headers.resize(resourceIndex.descriptorIndex + 1);

		headers[resourceIndex.descriptorIndex].startOffset = startOffset;
		headers[resourceIndex.descriptorIndex].dataSize = dataSize;
		headers[resourceIndex.descriptorIndex].resourceIndex = resourceIndex;
		headers[resourceIndex.descriptorIndex].specifics.framesLeft = 0;
	}
	else
	{
		DataHeader toAdd;
		toAdd.specifics.framesLeft = 0;
		toAdd.startOffset = headers.size() == 0 ? 0 :
			headers.back().startOffset + headers.back().dataSize;
		toAdd.dataSize = dataSize;
		toAdd.resourceIndex = resourceIndex;
		headers.push_back(toAdd);
	}

	usedDataSize += dataSize;

	if (initialData != nullptr)
		UpdateComponentData(resourceIndex, initialData);
}

void InternalBufferComponentData::RemoveComponent(const ResourceIndex& resourceIndex)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		std::int64_t difference = headers[resourceIndex.descriptorIndex].dataSize;
		difference = -difference;
		UpdateExistingHeaders(resourceIndex.descriptorIndex, difference);
		usedDataSize -= static_cast<size_t>(-difference);

		headers[resourceIndex.descriptorIndex].startOffset = static_cast<size_t>(-1);
		headers[resourceIndex.descriptorIndex].dataSize = 0;
		headers[resourceIndex.descriptorIndex].resourceIndex = ResourceIndex();
		headers[resourceIndex.descriptorIndex].specifics.framesLeft = 0;
	}
	else
	{
		for (size_t i = 0; i < headers.size(); ++i)
		{
			if (headers[i].resourceIndex.descriptorIndex != resourceIndex.descriptorIndex)
				continue;

			std::int64_t difference = headers[i].dataSize;
			difference = -difference;
			UpdateExistingHeaders(i, difference);
			usedDataSize -= static_cast<size_t>(-difference);
			size_t moveSize = sizeof(DataHeader) * (headers.size() - i - 1);
			std::memmove(headers.data() + i, headers.data() + i + 1, moveSize);
			headers.pop_back();
			return;
		}
	}
}

void InternalBufferComponentData::UpdateComponentData(const ResourceIndex& resourceIndex,
	void* dataPtr)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		unsigned char* destination = data.data();
		destination += headers[resourceIndex.descriptorIndex].startOffset;
		std::memcpy(destination, dataPtr, headers[resourceIndex.descriptorIndex].dataSize);
		headers[resourceIndex.descriptorIndex].specifics.framesLeft = nrOfFrames;
		updateNeeded = true;
	}
	else
	{
		for (auto& header : headers)
		{
			if (header.resourceIndex.descriptorIndex != resourceIndex.descriptorIndex)
				continue;

			unsigned char* destination = data.data();
			destination += header.startOffset;
			std::memcpy(destination, dataPtr, header.dataSize);
			header.specifics.framesLeft = nrOfFrames;
			updateNeeded = true;
			break;
		}
	}
}

void InternalBufferComponentData::PrepareUpdates(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers,
	BufferComponent& componentToUpdate)
{
	// A mapped resource is in an upload heap which needs to be generic read state
	if (updateNeeded == false || type == UpdateType::MAP_UPDATE)
		return;

	if (componentToUpdate.GetCurrentState() != D3D12_RESOURCE_STATE_COMMON &&
		componentToUpdate.GetCurrentState() != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		componentToUpdate.CreateTransitionBarrier(
			D3D12_RESOURCE_STATE_COPY_DEST, barriers);
	}
}

void InternalBufferComponentData::UpdateComponentResources(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader,
	BufferComponent& componentToUpdate, size_t componentAlignment)
{
	if (updateNeeded == false)
		return;

	updateNeeded = false;

	switch (type)
	{
	case UpdateType::INITIALISE_ONLY:
		HandleInitializeOnlyUpdate(commandList, uploader,
			componentToUpdate, componentAlignment);
		break;
	case UpdateType::MAP_UPDATE:
		HandleMapUpdate(componentToUpdate);
		break;
	case UpdateType::COPY_UPDATE:
		HandleCopyUpdate(commandList, uploader,
			componentToUpdate, componentAlignment);
		break;
	}
}