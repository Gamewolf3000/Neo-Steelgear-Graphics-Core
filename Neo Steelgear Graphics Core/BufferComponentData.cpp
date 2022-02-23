#include "BufferComponentData.h"

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
				data.resize(usedDataSize + dataSize); // Resize so new data fits

			usedDataSize += dataSize;
		}
	}
}

void BufferComponentData::RemoveComponent(ResourceIndex resourceIndex)
{
	if (type == UpdateType::NONE)
		return;

	if (type != UpdateType::INITIALISE_ONLY)
	{
		std::int64_t difference = headers[resourceIndex].dataSize;
		difference = -difference;
		headers[resourceIndex].specifics.framesLeft = 0;
		headers[resourceIndex].dataSize = 0;
		UpdateExistingHeaders(resourceIndex, difference);
	}
	else
	{
		for (size_t i = 0; i < headers.size(); ++i)
		{
			if (headers[i].resourceIndex != resourceIndex)
				continue;

			std::int64_t difference = headers[i].dataSize;
			difference = -difference;
			UpdateExistingHeaders(resourceIndex, difference);
			usedDataSize += difference;
			size_t moveSize = sizeof(DataHeader) * (headers.size() - i - 1);
			std::memmove(headers.data() + i, headers.data() + i + 1, moveSize);
			headers.pop_back();
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

	D3D12_RESOURCE_BARRIER barrier =
		componentToUpdate.CreateTransitionBarrier(D3D12_RESOURCE_STATE_COPY_DEST);

	if (barrier.Transition.StateBefore != barrier.Transition.StateAfter)
		barriers.push_back(barrier);
}

void BufferComponentData::UpdateComponentResources(
	ID3D12GraphicsCommandList* commandList, ResourceUploader& uploader,
	BufferComponent& componentToUpdate, size_t componentAlignment)
{
	if (updateNeeded == false)
		return;

	updateNeeded = false;

	for (size_t i = 0; i < headers.size(); ++i)
	{
		if (headers[i].specifics.framesLeft == 0)
			continue;
		else if (headers[i].specifics.framesLeft > 1)
			updateNeeded = true; // At least one update left for next frame

		auto handle = componentToUpdate.GetBufferHandle(headers[i].resourceIndex);
		unsigned char* source = data.data();
		source += headers[i].startOffset;

		if (type != UpdateType::MAP_UPDATE)
		{
			uploader.UploadBufferResourceData(handle.resource, commandList, source,
				handle.startOffset, headers[i].dataSize, componentAlignment);
		}
		else
		{
			componentToUpdate.UpdateMappedBuffer(headers[i].resourceIndex,
				source);
		}

		--headers[i].specifics.framesLeft;

		// If INITIALISE_ONLY is used and all frames are updated then we are finished with this one
		if (type == UpdateType::INITIALISE_ONLY && headers[i].specifics.framesLeft == 0)
			RemoveComponent(headers[i].resourceIndex);
	}
}