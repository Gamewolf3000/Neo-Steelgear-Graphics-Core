//#include "ComponentData.h"
//
//void ComponentData::UpdateExistingHeaders(size_t indexOfOriginalChange,
//	std::int64_t sizeDifference)
//{
//	size_t jointSize = 0;
//	for (size_t i = indexOfOriginalChange + 1; i < headers.size(); ++i)
//	{
//		headers[i].startOffset += sizeDifference;
//		jointSize += headers[i].dataSize;
//	}
//
//	unsigned char* destination = data.data();
//	destination += headers[indexOfOriginalChange].startOffset;
//	destination += headers[indexOfOriginalChange].dataSize;
//	unsigned char* source = destination - sizeDifference;
//	std::memcpy(destination, source, jointSize);
//}
//
//void ComponentData::Initialize(FrameType totalNrOfFrames,
//	UpdateType componentUpdateType, unsigned int totalSize)
//{
//	nrOfFrames = totalNrOfFrames;
//	type = componentUpdateType;
//	if (type != UpdateType::INITIALISE_ONLY)
//	{
//		data.resize(totalSize);
//		usedDataSize = totalSize;
//	}
//}
//
//void ComponentData::AddComponent(ResourceIndex resourceIndex, 
//	unsigned int dataSize)
//{
//	if (type != UpdateType::INITIALISE_ONLY && resourceIndex < headers.size())
//	{
//		std::int64_t difference = dataSize - headers[resourceIndex].dataSize;
//		headers[resourceIndex].dataSize = dataSize;
//		UpdateExistingHeaders(resourceIndex, difference);
//	}
//	else
//	{
//		DataHeader toAdd;
//		toAdd.framesLeft = 0;
//		toAdd.startOffset = headers.size() == 0 ? 0 :
//			headers.back().startOffset + headers.back().dataSize;
//		toAdd.dataSize = dataSize;
//		toAdd.resourceIndex = resourceIndex;
//		headers.push_back(toAdd);
//
//		if (type == UpdateType::INITIALISE_ONLY)
//		{
//			if (usedDataSize + dataSize > data.capacity())
//				data.resize(usedDataSize + dataSize); // Resize so new data fits
//
//			usedDataSize += dataSize;
//		}
//	}
//}
//
//void ComponentData::RemoveComponent(ResourceIndex resourceIndex)
//{
//	if (type != UpdateType::INITIALISE_ONLY)
//	{
//		std::int64_t difference = headers[resourceIndex].dataSize;
//		difference = -difference;
//		headers[resourceIndex].framesLeft = 0;
//		headers[resourceIndex].dataSize = 0;
//		UpdateExistingHeaders(resourceIndex, difference);
//	}
//	else
//	{
//		for (size_t i = 0; i < headers.size(); ++i)
//		{
//			if (headers[i].resourceIndex != resourceIndex)
//				continue;
//
//			std::int64_t difference = headers[i].dataSize;
//			difference = -difference;
//			UpdateExistingHeaders(resourceIndex, difference);
//			usedDataSize += difference;
//
//			for (; i < headers.size() - 1; ++i)
//				headers[i] = headers[i + 1];
//
//			headers.pop_back();
//		}
//	}
//}
//
//void ComponentData::UpdateComponentData(ResourceIndex resourceIndex, void* dataPtr)
//{
//	if (type != UpdateType::INITIALISE_ONLY)
//	{
//		unsigned char* destination = data.data();
//		destination += headers[resourceIndex].startOffset;
//		std::memcpy(destination, dataPtr, headers[resourceIndex].dataSize);
//		headers[resourceIndex].framesLeft = nrOfFrames;
//		updateNeeded = true;
//	}
//	else
//	{
//		for (auto& header : headers)
//		{
//			if (header.resourceIndex != resourceIndex)
//				continue;
//
//			unsigned char* destination = data.data();
//			destination += header.startOffset;
//			std::memcpy(destination, dataPtr, header.dataSize);
//			header.framesLeft = nrOfFrames;
//			updateNeeded = true;
//			break;
//		}
//	}
//}