#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/BufferComponentData.h"

#include "D3D12Helper.h"

TEST(BufferComponentDataTest, DefaultInitialisable)
{
	BufferComponentData componentData;
}

TEST(BufferComponentDataTest, RuntimeInitialisable)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 4> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::MAP_UPDATE, UpdateType::COPY_UPDATE };

	for (unsigned int frame = 1; frame < 11; ++frame)
	{
		for (auto& updateType : updateTypes)
		{
			for (unsigned int sizeExponent = 1; sizeExponent < 4; ++sizeExponent)
			{
				BufferComponentData componentData;
				componentData.Initialize(device, frame, updateType, 
					pow(100, sizeExponent));
			}
		}
	}

	device->Release();
}

TEST(BufferComponentDataTest, PerformsSimpleAddsCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 4> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::MAP_UPDATE, UpdateType::COPY_UPDATE };

	std::array<unsigned int, 7> componentsToAdd = { 1, 2, 4, 10, 20, 50, 100};

	for (unsigned int frame = 1; frame < 11; ++frame)
	{
		for (auto& updateType : updateTypes)
		{
			for (unsigned int sizeExponent = 1; sizeExponent < 4; ++sizeExponent)
			{
				size_t dataSize = pow(100, sizeExponent);

				for (auto& nrOfComponents : componentsToAdd)
				{
					BufferComponentData componentData;
					componentData.Initialize(device, frame, updateType, dataSize);
					size_t componentSize = dataSize / nrOfComponents;

					for (size_t i = 0; i < nrOfComponents; ++i)
					{
						componentData.AddComponent(ResourceIndex(i), 
							i * componentSize, componentSize);
						if (updateType != UpdateType::NONE)
						{
							unsigned char* startPtr = static_cast<unsigned char*>(
								componentData.GetComponentData(ResourceIndex(0)));
							void* current = 
								componentData.GetComponentData(ResourceIndex(i));
							ASSERT_EQ(startPtr + i * componentSize, current);
						}
					}
				}
			}
		}
	}

	device->Release();
}

TEST(BufferComponentDataTest, PerformsSimpleRemovesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 4> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::MAP_UPDATE, UpdateType::COPY_UPDATE };

	std::array<unsigned int, 7> componentsToAdd = { 1, 2, 4, 10, 20, 50, 100 };

	for (unsigned int frame = 1; frame < 11; ++frame)
	{
		for (auto& updateType : updateTypes)
		{
			for (unsigned int sizeExponent = 1; sizeExponent < 4; ++sizeExponent)
			{
				size_t dataSize = pow(100, sizeExponent);

				for (auto& nrOfComponents : componentsToAdd)
				{
					BufferComponentData componentData;
					componentData.Initialize(device, frame, updateType, dataSize);
					size_t componentSize = dataSize / nrOfComponents;

					for (size_t i = 0; i < nrOfComponents; ++i)
					{
						componentData.AddComponent(ResourceIndex(i),
							i * componentSize, componentSize);
					}

					for (size_t i = 0; i < nrOfComponents; ++i)
					{
						componentData.RemoveComponent(ResourceIndex(i));
					}

					for (size_t i = 0; i < nrOfComponents; ++i)
					{
						componentData.AddComponent(ResourceIndex(i),
							i * componentSize, componentSize);
						if (updateType != UpdateType::NONE)
						{
							unsigned char* startPtr = 
								static_cast<unsigned char*>(
									componentData.GetComponentData(
										ResourceIndex(0)));
							void* current =
								componentData.GetComponentData(ResourceIndex(i));
							ASSERT_EQ(startPtr + i * componentSize, current);
						}
					}
				}
			}
		}
	}

	device->Release();
}

TEST(BufferComponentDataTest, PerformsLocalUpdatesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 4> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::MAP_UPDATE, UpdateType::COPY_UPDATE };

	std::array<unsigned int, 7> componentsToAdd = { 1, 2, 4, 10, 20, 50, 100 };

	for (unsigned int frame = 1; frame < 11; ++frame)
	{
		for (auto& updateType : updateTypes)
		{
			for (unsigned int sizeExponent = 1; sizeExponent < 4; ++sizeExponent)
			{
				size_t dataSize = pow(100, sizeExponent);

				for (auto& nrOfComponents : componentsToAdd)
				{
					BufferComponentData componentData;
					componentData.Initialize(device, frame, updateType, dataSize);
					size_t componentSize = dataSize / nrOfComponents;
					unsigned char* data = new unsigned char[componentSize];

					for (size_t i = 0; i < nrOfComponents; ++i)
					{
						componentData.AddComponent(ResourceIndex(i),
							i * componentSize, componentSize);
						memset(data, static_cast<unsigned char>(i), componentSize);
						componentData.UpdateComponentData(ResourceIndex(i), data);

						if (updateType != UpdateType::NONE)
						{
							void* current =
								componentData.GetComponentData(ResourceIndex(i));							
							ASSERT_EQ(memcmp(current, data, componentSize), 0);
						}
					}
				}
			}
		}
	}

	device->Release();
}

void BufferComponentHelper(ID3D12Device* device, 
	SimpleCommandStructure& commandStructure, ResourceUploader& uploader,
	size_t nrOfAllocations, size_t allocationSize, unsigned int frames,
	std::function<void(ID3D12Device*, SimpleCommandStructure&, 
		ResourceUploader&, BufferComponentInfo&,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>&, AllowedViews&,
		BufferInfo&, size_t, size_t, unsigned int, bool)> func)
{
	UINT shaderBindableSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	UINT rtvSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	std::vector<BufferInfo> bufferInfos = { {1, allocationSize},
		{2, allocationSize}, {16, allocationSize}, {4, allocationSize}, 
		{64, allocationSize}, {32, allocationSize},{8, allocationSize},
		{1024, allocationSize} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
		{true, false, false, false}, {false, true, false, false},
		{true, true, false, false} };

	for (unsigned int bufferMultiplier = 16; bufferMultiplier < 21;
		bufferMultiplier++)
	{
		for (auto& allowedViews : ViewCombinations)
		{
			for (auto& bufferInfo : bufferInfos)
			{
				size_t alignedSize = ((bufferInfo.elementSize +
					(bufferInfo.alignment - 1)) & ~(bufferInfo.alignment - 1));

				if (alignedSize * nrOfAllocations > allocationSize * nrOfAllocations)
					continue; // We cannot fit all the buffers for this request

				bool mapped = !(allowedViews.rtv || allowedViews.uav);
				ResourceHeapInfo heapInfo(64 * 1024 * bufferMultiplier);
				BufferComponentInfo componentInfo = { bufferInfo, mapped,
					heapInfo };
				std::vector<DescriptorAllocationInfo<BufferViewDesc>>
					descriptorAllocationInfo;

				if (allowedViews.srv)
				{
					BufferViewDesc bufferSRVDesc(ViewType::SRV);
					DescriptorAllocationInfo<BufferViewDesc> srvInfo(
						ViewType::SRV, bufferSRVDesc, nrOfAllocations);
					descriptorAllocationInfo.push_back(srvInfo);
				}

				if (allowedViews.uav)
				{
					BufferViewDesc bufferUAVDesc(ViewType::UAV);
					DescriptorAllocationInfo<BufferViewDesc> uavInfo(
						ViewType::UAV, bufferUAVDesc, nrOfAllocations);
					descriptorAllocationInfo.push_back(uavInfo);
				}

				func(device, commandStructure, uploader, componentInfo,
					descriptorAllocationInfo, allowedViews, bufferInfo,
					nrOfAllocations, allocationSize, frames, mapped);
			}
		}
	}
}

void PrepareUpdateComponentData(std::function<void(
	ID3D12Device*, SimpleCommandStructure&, ResourceUploader&,
	BufferComponentInfo&, std::vector<DescriptorAllocationInfo<BufferViewDesc>>&,
	AllowedViews&, BufferInfo&, size_t, size_t, unsigned int, bool)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device))
		throw "Cannot proceed with tests as command structure cannot be created";

	std::array<unsigned int, 7> componentsToAdd = { 1, 2, 4, 10, 20, 50, 100 };

	for (unsigned int frameCount = 1; frameCount < 6; ++frameCount)
	{
		for (unsigned int sizeExponent = 1; sizeExponent < 4; ++sizeExponent)
		{
			size_t dataSize = pow(100, sizeExponent);
			ResourceUploader uploader;
			uploader.Initialize(device, dataSize, AllocationStrategy::FIRST_FIT);

			for (auto& nrOfComponents : componentsToAdd)
			{
				size_t componentSize = dataSize / nrOfComponents;
				BufferComponentHelper(device, commandStructure, uploader, 
					nrOfComponents, componentSize, frameCount, func);
			}
		}
	}

	device->Release();
}

TEST(BufferComponentDataTest, PerformsMapUpdatesCorrectly)
{
	auto lambda = [](ID3D12Device* device, SimpleCommandStructure& commandStructure,
		ResourceUploader& uploader, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo,
		AllowedViews& allowedViews, BufferInfo& bufferInfo, size_t nrOfAllocations,
		size_t allocationSize, unsigned int frames, bool mapped)
	{
		if (!mapped)
			return;

		unsigned char* data = new unsigned char[allocationSize * nrOfAllocations];

		ID3D12Resource* readbackBuffer = CreateBuffer(device,
			allocationSize * nrOfAllocations, true);
		if (readbackBuffer == nullptr)
			throw "Cannot proceed with tests as readback buffer cannot be created";

		size_t currentFenceValue = 0;
		ID3D12Fence* fence = CreateFence(device, currentFenceValue, 
			D3D12_FENCE_FLAG_NONE);

		BufferComponentData componentData;
		componentData.Initialize(device, frames, UpdateType::MAP_UPDATE,
			componentInfo.heapInfo.info.owned.heapSize);

		std::vector<BufferComponent> components(frames);
		std::vector<ResourceIndex> resourceIndices(nrOfAllocations);

		for (unsigned int currentFrame = 0; currentFrame < frames; ++currentFrame)
		{
			components[currentFrame].Initialize(device, componentInfo,
				descriptorInfo);

			for (size_t i = 0; i < nrOfAllocations; ++i)
			{
				resourceIndices[i] = components[currentFrame].CreateBuffer(
						allocationSize / bufferInfo.elementSize);
			}
		}

		for (size_t i = 0; i < nrOfAllocations; ++i)
		{
			size_t dataOffset = i * allocationSize;
			memset(data + dataOffset, static_cast<unsigned char>(i),
				allocationSize);

			auto handle = components[0].GetBufferHandle(resourceIndices[i]);
			componentData.AddComponent(resourceIndices[i],
				handle.startOffset, allocationSize);
			componentData.UpdateComponentData(resourceIndices[i],
				data + dataOffset);
		}

		for (unsigned int currentFrame = 0; currentFrame < frames; ++currentFrame)
		{
			componentData.UpdateComponentResources(commandStructure.list, uploader,
				components[currentFrame], bufferInfo.alignment);

			for (size_t i = 0; i < nrOfAllocations; ++i)
			{
				size_t currentOffset = i * allocationSize;
				auto handle = components[currentFrame].GetBufferHandle(
					resourceIndices[i]);
				commandStructure.list->CopyBufferRegion(readbackBuffer, currentOffset,
					handle.resource, handle.startOffset, allocationSize);
			}

			ExecuteGraphicsCommandList(commandStructure.list, 
				commandStructure.queue);
			FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
			CheckResourceData(readbackBuffer, 0, data, 0, 
				allocationSize * nrOfAllocations);

			uploader.RestoreUsedMemory();

			if (FAILED(commandStructure.allocator->Reset()))
				throw "Cannot proceed with tests as command allocator cannot be reset";

			if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
				throw "Cannot proceed with tests as command list cannot be reset";
		}

		delete[] data;
		fence->Release();
		readbackBuffer->Release();
	};

	PrepareUpdateComponentData(lambda);
}

TEST(BufferComponentDataTest, PerformsCopyUpdatesCorrectly)
{
	auto lambda = [](ID3D12Device* device, SimpleCommandStructure& commandStructure,
		ResourceUploader& uploader, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo,
		AllowedViews& allowedViews, BufferInfo& bufferInfo, size_t nrOfAllocations,
		size_t allocationSize, unsigned int frames, bool mapped)
	{
		if (mapped)
			return;

		unsigned char* data = new unsigned char[allocationSize * nrOfAllocations];

		ID3D12Resource* readbackBuffer = CreateBuffer(device,
			allocationSize * nrOfAllocations, true);
		if (readbackBuffer == nullptr)
			throw "Cannot proceed with tests as readback buffer cannot be created";

		size_t currentFenceValue = 0;
		ID3D12Fence* fence = CreateFence(device, currentFenceValue,
			D3D12_FENCE_FLAG_NONE);

		BufferComponentData componentData;
		componentData.Initialize(device, frames, UpdateType::COPY_UPDATE,
			componentInfo.heapInfo.info.owned.heapSize);

		std::vector<BufferComponent> components(frames);
		std::vector<ResourceIndex> resourceIndices(nrOfAllocations);

		for (unsigned int currentFrame = 0; currentFrame < frames; ++currentFrame)
		{
			components[currentFrame].Initialize(device, componentInfo,
				descriptorInfo);

			for (size_t i = 0; i < nrOfAllocations; ++i)
			{
				resourceIndices[i] = components[currentFrame].CreateBuffer(
					allocationSize / bufferInfo.elementSize);
			}
		}

		for (size_t i = 0; i < nrOfAllocations; ++i)
		{
			size_t dataOffset = i * allocationSize;
			memset(data + dataOffset, static_cast<unsigned char>(i),
				allocationSize);

			auto handle = components[0].GetBufferHandle(resourceIndices[i]);
			componentData.AddComponent(resourceIndices[i],
				handle.startOffset, allocationSize);
			componentData.UpdateComponentData(resourceIndices[i],
				data + dataOffset);
		}

		for (unsigned int currentFrame = 0; currentFrame < frames; ++currentFrame)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier = components[currentFrame].CreateTransitionBarrier(
				D3D12_RESOURCE_STATE_COPY_DEST);
			commandStructure.list->ResourceBarrier(1, &barrier);

			componentData.UpdateComponentResources(commandStructure.list, uploader,
				components[currentFrame], bufferInfo.alignment);

			barrier = components[currentFrame].CreateTransitionBarrier(
				D3D12_RESOURCE_STATE_COPY_SOURCE);
			commandStructure.list->ResourceBarrier(1, &barrier);

			for (size_t i = 0; i < nrOfAllocations; ++i)
			{
				size_t currentOffset = i * allocationSize;
				auto handle = components[currentFrame].GetBufferHandle(
					resourceIndices[i]);
				commandStructure.list->CopyBufferRegion(readbackBuffer, currentOffset,
					handle.resource, handle.startOffset, allocationSize);
			}

			ExecuteGraphicsCommandList(commandStructure.list,
				commandStructure.queue);
			FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
			CheckResourceData(readbackBuffer, 0, data, 0,
				allocationSize * nrOfAllocations);

			uploader.RestoreUsedMemory();

			if (FAILED(commandStructure.allocator->Reset()))
				throw "Cannot proceed with tests as command allocator cannot be reset";

			if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
				throw "Cannot proceed with tests as command list cannot be reset";
		}

		delete[] data;
		fence->Release();
		readbackBuffer->Release();
	};

	PrepareUpdateComponentData(lambda);
}

TEST(BufferComponentDataTest, PerformsInitializeOnlyUpdatesCorrectly)
{
	auto lambda = [](ID3D12Device* device, SimpleCommandStructure& commandStructure,
		ResourceUploader& uploader, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo,
		AllowedViews& allowedViews, BufferInfo& bufferInfo, size_t nrOfAllocations,
		size_t allocationSize, unsigned int frames, bool mapped)
	{
		if (mapped)
			return;

		unsigned char* data = new unsigned char[allocationSize * nrOfAllocations];

		ID3D12Resource* readbackBuffer = CreateBuffer(device,
			allocationSize * nrOfAllocations, true);
		if (readbackBuffer == nullptr)
			throw "Cannot proceed with tests as readback buffer cannot be created";

		size_t currentFenceValue = 0;
		ID3D12Fence* fence = CreateFence(device, currentFenceValue,
			D3D12_FENCE_FLAG_NONE);

		BufferComponentData componentData;
		componentData.Initialize(device, frames, UpdateType::INITIALISE_ONLY,
			componentInfo.heapInfo.info.owned.heapSize);

		std::vector<BufferComponent> components(frames);
		std::vector<ResourceIndex> resourceIndices(nrOfAllocations);

		for (unsigned int currentFrame = 0; currentFrame < frames; ++currentFrame)
		{
			components[currentFrame].Initialize(device, componentInfo,
				descriptorInfo);

			for (size_t i = 0; i < nrOfAllocations; ++i)
			{
				resourceIndices[i] = components[currentFrame].CreateBuffer(
					allocationSize / bufferInfo.elementSize);
			}
		}

		for (size_t i = 0; i < nrOfAllocations; ++i)
		{
			size_t dataOffset = i * allocationSize;
			memset(data + dataOffset, static_cast<unsigned char>(i),
				allocationSize);

			auto handle = components[0].GetBufferHandle(resourceIndices[i]);
			componentData.AddComponent(resourceIndices[i],
				handle.startOffset, allocationSize);
			componentData.UpdateComponentData(resourceIndices[i],
				data + dataOffset);
		}

		for (unsigned int currentFrame = 0; currentFrame < frames; ++currentFrame)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier = components[currentFrame].CreateTransitionBarrier(
				D3D12_RESOURCE_STATE_COPY_DEST);
			commandStructure.list->ResourceBarrier(1, &barrier);

			componentData.UpdateComponentResources(commandStructure.list, uploader,
				components[currentFrame], bufferInfo.alignment);

			barrier = components[currentFrame].CreateTransitionBarrier(
				D3D12_RESOURCE_STATE_COPY_SOURCE);
			commandStructure.list->ResourceBarrier(1, &barrier);

			for (size_t i = 0; i < nrOfAllocations; ++i)
			{
				size_t currentOffset = i * allocationSize;
				auto handle = components[currentFrame].GetBufferHandle(
					resourceIndices[i]);
				commandStructure.list->CopyBufferRegion(readbackBuffer, currentOffset,
					handle.resource, handle.startOffset, allocationSize);
			}

			ExecuteGraphicsCommandList(commandStructure.list,
				commandStructure.queue);
			FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
			CheckResourceData(readbackBuffer, 0, data, 0,
				allocationSize * nrOfAllocations);

			uploader.RestoreUsedMemory();

			if (FAILED(commandStructure.allocator->Reset()))
				throw "Cannot proceed with tests as command allocator cannot be reset";

			if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
				throw "Cannot proceed with tests as command list cannot be reset";
		}

		delete[] data;
		fence->Release();
		readbackBuffer->Release();
	};

	PrepareUpdateComponentData(lambda);
}