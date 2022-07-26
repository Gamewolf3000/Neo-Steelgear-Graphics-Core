#pragma once

#include "InternalBufferComponentData.h"

class BufferComponentData
{
private:
	ResourceIndex TranslateIndexToInternal(const ResourceIndex& original);

	std::vector<InternalBufferComponentData> internalData;
	ID3D12Device* device = nullptr;
	FrameType nrOfFrames = 0;
	UpdateType updateType = UpdateType::NONE;
	unsigned int expansionSize = 0;

public:
	BufferComponentData() = default;
	~BufferComponentData() = default;
	BufferComponentData(const BufferComponentData & other) = delete;
	BufferComponentData& operator=(const BufferComponentData & other) = delete;
	BufferComponentData(BufferComponentData && other) = default;
	BufferComponentData& operator=(BufferComponentData && other) = default;

	void Initialize(ID3D12Device* deviceToUse, FrameType totalNrOfFrames,
		UpdateType componentUpdateType, unsigned int initialSize,
		unsigned int sizeWhenExpanding);

	void AddComponent(const ResourceIndex& resourceIndex, size_t startOffset,
		unsigned int dataSize, void* initialData = nullptr);
	void RemoveComponent(const ResourceIndex& resourceIndex);
	void UpdateComponentData(const ResourceIndex& resourceIndex, void* dataPtr);

	void PrepareUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		BufferComponent& componentToUpdate);
	void UpdateComponentResources(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);

	void* GetComponentData(const ResourceIndex& resourceIndex);
};