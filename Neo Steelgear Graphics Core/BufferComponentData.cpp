#include "BufferComponentData.h"

void BufferComponentData::HandleInitializeOnlyUpdate(
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

		if (type != UpdateType::MAP_UPDATE)
		{
			bool result = uploader.UploadBufferResourceData(handle.resource,
				commandList, source, handle.startOffset, headers[i].dataSize,
				componentAlignment);

			if (!result)
				headers[i].specifics.framesLeft += nrOfFrames; // Let it loop, not the best solution
		}
		else
		{
			componentToUpdate.UpdateMappedBuffer(headers[i].resourceIndex,
				source);
		}

		--headers[i].specifics.framesLeft;

		// If INITIALISE_ONLY is used and all frames are updated then we are finished with this one
		if (type == UpdateType::INITIALISE_ONLY && headers[i].specifics.framesLeft == 0)
		{
			RemoveComponent(headers[i].resourceIndex);
			--i;
		}
	}
}

void BufferComponentData::HandleCopyUpdate(
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

void BufferComponentData::HandleMapUpdate(BufferComponent& componentToUpdate)
{
	for (auto& header : headers)
	{
		if (header.specifics.framesLeft == 0)
			continue;
		else if (header.specifics.framesLeft > 1)
			updateNeeded = true; // At least one update left for next frame

		componentToUpdate.UpdateMappedBuffer(header.resourceIndex,
			data.data() + header.startOffset);
	}
}

void BufferComponentData::AddComponent(ResourceIndex resourceIndex,
	unsigned int dataSize)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY && resourceIndex < headers.size())
	{
		std::int64_t difference = dataSize - headers[resourceIndex].dataSize;
		headers[resourceIndex].dataSize = dataSize;
		UpdateExistingHeaders(resourceIndex, difference);
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

void BufferComponentData::AddComponent(ResourceIndex resourceIndex,
	size_t startOffset, unsigned int dataSize, void* initialData)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		if(headers.size() <= resourceIndex)
			headers.resize(resourceIndex + 1);

		headers[resourceIndex].startOffset = startOffset;
		headers[resourceIndex].dataSize = dataSize;
		headers[resourceIndex].resourceIndex = resourceIndex;
		headers[resourceIndex].specifics.framesLeft = 0;
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

	if (initialData != nullptr)
		UpdateComponentData(resourceIndex, initialData);
}

void BufferComponentData::RemoveComponent(ResourceIndex resourceIndex)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		headers[resourceIndex].startOffset = static_cast<size_t>(-1);
		headers[resourceIndex].dataSize = 0;
		headers[resourceIndex].resourceIndex = ResourceIndex(-1);
		headers[resourceIndex].specifics.framesLeft = 0;
	}
	else
	{
		for (size_t i = 0; i < headers.size(); ++i)
		{
			if (headers[i].resourceIndex != resourceIndex)
				continue;

			std::int64_t difference = headers[i].dataSize;
			difference = -difference;
			UpdateExistingHeaders(i, difference);
			usedDataSize = difference >= 0 ? 
				usedDataSize + static_cast<size_t>(difference) :
				usedDataSize - static_cast<size_t>(-difference);
			size_t moveSize = sizeof(DataHeader) * (headers.size() - i - 1);
			std::memmove(headers.data() + i, headers.data() + i + 1, moveSize);
			headers.pop_back();
			return;
		}
	}
}

void BufferComponentData::UpdateComponentData(ResourceIndex resourceIndex,
	void* dataPtr)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		unsigned char* destination = data.data();
		destination += headers[resourceIndex].startOffset;
		std::memcpy(destination, dataPtr, headers[resourceIndex].dataSize);
		headers[resourceIndex].specifics.framesLeft = nrOfFrames;
		updateNeeded = true;
	}
	else
	{
		for (auto& header : headers)
		{
			if (header.resourceIndex != resourceIndex)
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

void BufferComponentData::PrepareUpdates(
	std::vector<D3D12_RESOURCE_BARRIER>& barriers,
	BufferComponent& componentToUpdate)
{
	// A mapped resource is in an upload heap which needs to be generic read state
	if (updateNeeded == false || type == UpdateType::MAP_UPDATE)
		return;

	if (componentToUpdate.GetCurrentState() != D3D12_RESOURCE_STATE_COMMON &&
		componentToUpdate.GetCurrentState() != D3D12_RESOURCE_STATE_COPY_DEST)
	{
		D3D12_RESOURCE_BARRIER barrier = 
			componentToUpdate.CreateTransitionBarrier(
				D3D12_RESOURCE_STATE_COPY_DEST);

		barriers.push_back(barrier);
	}
}

void BufferComponentData::UpdateComponentResources(
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