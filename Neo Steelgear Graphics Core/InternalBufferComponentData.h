#pragma once

#include "ComponentData.h"
#include "BufferComponent.h"
#include "ResourceUploader.h"

struct BufferSpecific
{
	FrameType framesLeft;
};

class InternalBufferComponentData : public ComponentData<BufferSpecific>
{
private:
	void HandleInitializeOnlyUpdate(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);
	void HandleCopyUpdate(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);
	void HandleMapUpdate(BufferComponent& componentToUpdate);

public:
	InternalBufferComponentData() = default;
	~InternalBufferComponentData() = default;
	InternalBufferComponentData(const InternalBufferComponentData& other) = delete;
	InternalBufferComponentData& operator=(const InternalBufferComponentData& other) = delete;
	InternalBufferComponentData(InternalBufferComponentData&& other) = default;
	InternalBufferComponentData& operator=(InternalBufferComponentData&& other) = default;

	//void AddComponent(ResourceIndex resourceIndex, unsigned int dataSize);
	void AddComponent(const ResourceIndex& resourceIndex, size_t startOffset,
		unsigned int dataSize, void* initialData = nullptr);
	void RemoveComponent(const ResourceIndex& resourceIndex) override;
	void UpdateComponentData(const ResourceIndex& resourceIndex, void* dataPtr);

	void PrepareUpdates(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		BufferComponent& componentToUpdate);
	void UpdateComponentResources(ID3D12GraphicsCommandList* commandList,
		ResourceUploader& uploader, BufferComponent& componentToUpdate,
		size_t componentAlignment);
};