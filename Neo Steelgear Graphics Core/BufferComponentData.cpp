#include "BufferComponentData.h"

ResourceIndex BufferComponentData::TranslateIndexToInternal(
    const ResourceIndex& original)
{
    ResourceIndex toReturn = original;
    toReturn.descriptorIndex = original.allocatorIdentifier.internalIndex;
    return toReturn;
}

void BufferComponentData::Initialize(ID3D12Device* deviceToUse,
    FrameType totalNrOfFrames, UpdateType componentUpdateType,
    unsigned int initialSize, unsigned int sizeWhenExpanding)
{
    device = deviceToUse;
    nrOfFrames = totalNrOfFrames;
    updateType = componentUpdateType;
    expansionSize = sizeWhenExpanding;
    internalData.push_back(InternalBufferComponentData());
    internalData[0].Initialize(device, nrOfFrames, updateType, initialSize);
}

void BufferComponentData::AddComponent(const ResourceIndex& resourceIndex,
    size_t startOffset, unsigned int dataSize, void* initialData)
{
    size_t vectorIndex = resourceIndex.allocatorIdentifier.heapChunkIndex;

    if (vectorIndex >= internalData.size())
    {
        internalData.push_back(InternalBufferComponentData());
        internalData.back().Initialize(device, nrOfFrames, updateType, expansionSize);
    }

    ResourceIndex translatedIndex = TranslateIndexToInternal(resourceIndex);
    internalData[vectorIndex].AddComponent(translatedIndex, startOffset, dataSize, initialData);
}

void BufferComponentData::RemoveComponent(const ResourceIndex& resourceIndex)
{
    size_t vectorIndex = resourceIndex.allocatorIdentifier.heapChunkIndex;
    ResourceIndex translatedIndex = TranslateIndexToInternal(resourceIndex);
    internalData[vectorIndex].RemoveComponent(translatedIndex);
}

void BufferComponentData::UpdateComponentData(const ResourceIndex& resourceIndex,
    void* dataPtr)
{
    size_t vectorIndex = resourceIndex.allocatorIdentifier.heapChunkIndex;
    ResourceIndex translatedIndex = TranslateIndexToInternal(resourceIndex);
    internalData[vectorIndex].UpdateComponentData(translatedIndex, dataPtr);
}

void BufferComponentData::PrepareUpdates(
    std::vector<D3D12_RESOURCE_BARRIER>& barriers, BufferComponent& componentToUpdate)
{
    for (auto& data : internalData)
        data.PrepareUpdates(barriers, componentToUpdate);
}

void BufferComponentData::UpdateComponentResources(ID3D12GraphicsCommandList* commandList,
    ResourceUploader& uploader, BufferComponent& componentToUpdate, size_t componentAlignment)
{
    for (auto& data : internalData)
        data.UpdateComponentResources(commandList, uploader, componentToUpdate, componentAlignment);
}

void* BufferComponentData::GetComponentData(const ResourceIndex& resourceIndex)
{
    size_t vectorIndex = resourceIndex.allocatorIdentifier.heapChunkIndex;
    ResourceIndex translatedIndex = TranslateIndexToInternal(resourceIndex);
    return internalData[vectorIndex].GetComponentData(translatedIndex);
}
