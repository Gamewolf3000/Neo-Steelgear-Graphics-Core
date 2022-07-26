#include "pch.h"

#include <array>
#include <utility>
#include <functional>

#include "../Neo Steelgear Graphics Core/BufferComponent.h"
#include "../Neo Steelgear Graphics Core/MultiHeapAllocatorGPU.h"

#include "D3D12Helper.h"

TEST(BufferComponentTest, DefaultInitialisable)
{
	BufferComponent bufferComponent;
}

void InitializationWrapper(std::function<void(ID3D12Device*, BufferComponentInfo&,
	std::vector<DescriptorAllocationInfo<BufferViewDesc>>&, AllowedViews&,
	BufferInfo&, size_t)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	UINT shaderBindableSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	UINT rtvSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	std::vector<BufferInfo> bufferInfos = { {1, 1}, {1, 32}, {2, 2}, {16, 16},
		{4, 8}, {64, 64}, {32, 128}, {2, 256}, {4, 4}, {8, 32}, {1024, 4096} };

	std::vector<AllowedViews> ViewCombinations = { {true, false, false, false},
		{false, true, false, false}, {true, true, false, false} };

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 11;
		bufferMultiplier++)
	{
		for (auto& allowedViews : ViewCombinations)
		{
			for (auto& bufferInfo : bufferInfos)
			{
				bool mapped = !(allowedViews.rtv || allowedViews.uav);
				ResourceComponentMemoryInfo memoryInfo;
				memoryInfo.initialMinimumHeapSize = 64 * 1024 * bufferMultiplier;
				memoryInfo.expansionMinimumSize = 64 * 1024;
				memoryInfo.heapAllocator = &heapAllocator;
				BufferComponentInfo componentInfo = { bufferInfo, mapped,
					memoryInfo };
				std::vector<DescriptorAllocationInfo<BufferViewDesc>>
					descriptorAllocationInfo;

				size_t nrOfAllocations = min(1000,
					(64 * 1024 * bufferMultiplier) / bufferInfo.elementSize);

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

				func(device, componentInfo, descriptorAllocationInfo,
					allowedViews, bufferInfo, nrOfAllocations);
			}
		}
	}

	device->Release();
}

TEST(BufferComponentTest, RuntimeInitialisable)
{
	auto func = [](ID3D12Device* device, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
		AllowedViews& views, BufferInfo& info, size_t maxAllocations)
	{
		BufferComponent component;
		component.Initialize(device, componentInfo, descriptorAllocationInfo);
	};

	InitializationWrapper(func);
}

TEST(BufferComponentTest, CorrectlyCreatesSimpleBuffers)
{
	auto func = [](ID3D12Device* device, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
		AllowedViews& views, BufferInfo& info, size_t maxAllocations)
	{
		std::array<size_t, 7> elementsPerAllocation = { 1, 2, 4, 8, 16, 32, 64 };
		ResourceIndex startIndex;
		startIndex.allocatorIdentifier.heapChunkIndex = 0;
		startIndex.allocatorIdentifier.internalIndex = 0;
		startIndex.descriptorIndex = 0;

		for (auto& nrOfElements : elementsPerAllocation)
		{
			BufferComponent component;
			component.Initialize(device, componentInfo, descriptorAllocationInfo);

			for (unsigned int i = 0; i < maxAllocations / nrOfElements; ++i)
			{
				auto currentIndex = component.CreateBuffer(nrOfElements);
				ASSERT_EQ(currentIndex.allocatorIdentifier.heapChunkIndex, 0);
				ASSERT_EQ(currentIndex.allocatorIdentifier.internalIndex, i);
				ASSERT_EQ(currentIndex.descriptorIndex, i);
				auto start = component.GetBufferHandle(startIndex);
				auto current = component.GetBufferHandle(currentIndex);
				ASSERT_EQ(start.startOffset + i * nrOfElements * info.elementSize,
					current.startOffset);
			}
		}
	};

	InitializationWrapper(func);
}

TEST(BufferComponentTest, CorrectlyRemovesSimpleBuffers)
{
	auto func = [](ID3D12Device* device, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
		AllowedViews& views, BufferInfo& info, size_t maxAllocations)
	{
		std::array<size_t, 7> elementsPerAllocation = { 1, 2, 4, 8, 16, 32, 64 };

		for (auto& nrOfElements : elementsPerAllocation)
		{
			BufferComponent component;
			component.Initialize(device, componentInfo, descriptorAllocationInfo);

			for (unsigned int i = 0; i < maxAllocations / nrOfElements; ++i)
			{
				auto currentIndex = component.CreateBuffer(nrOfElements);
				ASSERT_EQ(currentIndex.allocatorIdentifier.heapChunkIndex, 0);
				ASSERT_EQ(currentIndex.allocatorIdentifier.internalIndex, i);
				ASSERT_EQ(currentIndex.descriptorIndex, i);
			}

			for (unsigned int i = 0; i < maxAllocations / nrOfElements; ++i)
			{
				ResourceIndex indexToRemove;
				indexToRemove.allocatorIdentifier.heapChunkIndex = 0;
				indexToRemove.allocatorIdentifier.internalIndex = i;
				indexToRemove.descriptorIndex = i;
				component.RemoveComponent(indexToRemove);
			}

			size_t maxIndex(maxAllocations / nrOfElements);
			for (unsigned int i = 0; i < maxAllocations / nrOfElements; ++i)
			{
				auto currentIndex = component.CreateBuffer(nrOfElements);
				ASSERT_EQ(currentIndex.allocatorIdentifier.heapChunkIndex, 0);
				ASSERT_LE(currentIndex.allocatorIdentifier.internalIndex, maxIndex);
				ASSERT_LE(currentIndex.descriptorIndex, maxIndex);
			}
		}
	};

	InitializationWrapper(func);
}

TEST(BufferComponentTest, CorrectlyUpdatesMappedBuffers)
{
	auto func = [](ID3D12Device* device, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
		AllowedViews& views, BufferInfo& info, size_t maxAllocations)
	{
		if (!componentInfo.mappedResource)
			return;

		size_t allocationSize = componentInfo.bufferInfo.elementSize *
			maxAllocations;
		unsigned char* data = new unsigned char[allocationSize];

		SimpleCommandStructure commandStructure;
		if (!CreateSimpleCommandStructure(commandStructure, device))
			FAIL() << "Cannot proceed with tests as command structure cannot be created";

		commandStructure.list->Close();
		ID3D12Resource* readbackBuffer = CreateBuffer(device, allocationSize, true);
		if (readbackBuffer == nullptr)
			FAIL() << "Cannot proceed with tests as a resource could not be created";

		size_t currentFenceValue = 0;
		ID3D12Fence* fence = CreateFence(device, currentFenceValue,
			D3D12_FENCE_FLAG_NONE);
		if (fence == nullptr)
			FAIL() << "Cannot proceed with tests as a fence could not be created";

		std::array<size_t, 7> elementsPerAllocation = { 1, 2, 4, 8, 16, 32, 64 };

		for (auto& nrOfElements : elementsPerAllocation)
		{
			BufferComponent component;
			component.Initialize(device, componentInfo, descriptorAllocationInfo);
			size_t bufferSize = componentInfo.bufferInfo.elementSize * nrOfElements;

			for (unsigned int i = 0; i < maxAllocations / nrOfElements; ++i)
			{
				void* currentDataStart = data + i * bufferSize;
				memset(currentDataStart, static_cast<unsigned char>(i), bufferSize);
				auto currentIndex = component.CreateBuffer(nrOfElements);
				ASSERT_EQ(currentIndex.allocatorIdentifier.heapChunkIndex, 0);
				ASSERT_EQ(currentIndex.allocatorIdentifier.internalIndex, i);
				ASSERT_EQ(currentIndex.descriptorIndex, i);
				component.UpdateMappedBuffer(currentIndex, currentDataStart);
			}

			if (FAILED(commandStructure.allocator->Reset()))
				FAIL() << "Cannot proceed with tests since a command allocator could not be reset";

			if (FAILED(commandStructure.list->Reset(
				commandStructure.allocator, nullptr)))
			{
				FAIL() << "Cannot proceed with tests since a command allocator could not be reset";
			}

			ResourceIndex startIndex;
			startIndex.allocatorIdentifier.heapChunkIndex = 0;
			startIndex.allocatorIdentifier.internalIndex = 0;
			startIndex.descriptorIndex = 0;
			commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
				component.GetBufferHandle(startIndex).resource, 0, allocationSize);
			ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
			FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
			CheckResourceData(readbackBuffer, 0, data, 0, 
				(maxAllocations / nrOfElements) * bufferSize); // All memory might not be touched, ignore untouched memory
		}

		delete[] data;
		readbackBuffer->Release();
		fence->Release();
	};

	InitializationWrapper(func);
}

TEST(BufferComponentTest, ExpandsCorrectly)
{
	auto func = [](ID3D12Device* device, BufferComponentInfo& componentInfo,
		std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
		AllowedViews& views, BufferInfo& info, size_t maxAllocations)
	{
		if (!componentInfo.mappedResource || maxAllocations == 1000) //1000 means we have more memory
			return;

		unsigned int expectedNrOfExpansions = 4;
		size_t allocationSize = componentInfo.bufferInfo.elementSize *
			maxAllocations * (expectedNrOfExpansions + 1); // + 1 for original
		unsigned char* data = new unsigned char[allocationSize];

		SimpleCommandStructure commandStructure;
		if (!CreateSimpleCommandStructure(commandStructure, device))
			FAIL() << "Cannot proceed with tests as command structure cannot be created";

		commandStructure.list->Close();
		ID3D12Resource* readbackBuffer = CreateBuffer(device, allocationSize, true);
		if (readbackBuffer == nullptr)
			FAIL() << "Cannot proceed with tests as a resource could not be created";

		size_t currentFenceValue = 0;
		ID3D12Fence* fence = CreateFence(device, currentFenceValue,
			D3D12_FENCE_FLAG_NONE);
		if (fence == nullptr)
			FAIL() << "Cannot proceed with tests as a fence could not be created";

		std::array<size_t, 7> elementsPerAllocation = { 1, 2, 4, 8, 16, 32, 64 };

		for (auto& nrOfElements : elementsPerAllocation)
		{
			if (nrOfElements > maxAllocations)
				continue;

			BufferComponent component;
			component.Initialize(device, componentInfo, descriptorAllocationInfo);
			size_t bufferSize = componentInfo.bufferInfo.elementSize * nrOfElements;

			for (unsigned int i = 0; i < maxAllocations / nrOfElements; ++i)
			{
				void* currentDataStart = data + i * bufferSize;
				memset(currentDataStart, static_cast<unsigned char>(i), bufferSize);
				auto currentIndex = component.CreateBuffer(nrOfElements);
				ASSERT_EQ(currentIndex.allocatorIdentifier.heapChunkIndex, 0);
				ASSERT_EQ(currentIndex.allocatorIdentifier.internalIndex, i);
				ASSERT_EQ(currentIndex.descriptorIndex, i);
				component.UpdateMappedBuffer(currentIndex, currentDataStart);
			}

			unsigned int secondaryOffset = maxAllocations / nrOfElements;
			size_t currentExpectedInternalIndex = 0;
			std::vector<std::pair<size_t, size_t>> chunkInfos;
			chunkInfos.push_back(std::make_pair(1, 0));
			for (unsigned int i = 0; 
				i < (maxAllocations / nrOfElements) * expectedNrOfExpansions; ++i)
			{
				void* currentDataStart = data + (secondaryOffset + i) * bufferSize;
				memset(currentDataStart, 
					static_cast<unsigned char>(i + secondaryOffset), bufferSize);
				auto currentIndex = component.CreateBuffer(nrOfElements);

				if (chunkInfos.back().first != 
					currentIndex.allocatorIdentifier.heapChunkIndex)
				{
					currentExpectedInternalIndex = 0;
					chunkInfos.push_back(std::make_pair(chunkInfos.size() + 1, 0));
				}

				ASSERT_GE(currentIndex.allocatorIdentifier.heapChunkIndex, 1);
				ASSERT_EQ(currentIndex.allocatorIdentifier.internalIndex, 
					currentExpectedInternalIndex);
				ASSERT_EQ(currentIndex.descriptorIndex, i + secondaryOffset);
				component.UpdateMappedBuffer(currentIndex, currentDataStart);

				++currentExpectedInternalIndex;
				chunkInfos.back().second += nrOfElements * info.elementSize;
			}

			if (FAILED(commandStructure.allocator->Reset()))
				FAIL() << "Cannot proceed with tests since a command allocator could not be reset";

			if (FAILED(commandStructure.list->Reset(
				commandStructure.allocator, nullptr)))
			{
				FAIL() << "Cannot proceed with tests since a command allocator could not be reset";
			}

			ResourceIndex startIndex;
			startIndex.allocatorIdentifier.heapChunkIndex = 0;
			startIndex.allocatorIdentifier.internalIndex = 0;
			startIndex.descriptorIndex = 0;
			commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
				component.GetBufferHandle(startIndex).resource, 0, 
				allocationSize / (expectedNrOfExpansions + 1));
			ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
			FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
			CheckResourceData(readbackBuffer, 0, data, 0,
				(maxAllocations / nrOfElements) * bufferSize);

			unsigned char* currentDataStart = data +
				(maxAllocations / nrOfElements) * bufferSize;
			for (auto& chunk : chunkInfos)
			{
				if (FAILED(commandStructure.allocator->Reset()))
					FAIL() << "Cannot proceed with tests since a command allocator could not be reset";

				if (FAILED(commandStructure.list->Reset(
					commandStructure.allocator, nullptr)))
				{
					FAIL() << "Cannot proceed with tests since a command allocator could not be reset";
				}

				ResourceIndex startIndex;
				startIndex.allocatorIdentifier.heapChunkIndex = chunk.first;
				startIndex.allocatorIdentifier.internalIndex = 0;
				startIndex.descriptorIndex = 0;
				commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
					component.GetBufferHandle(startIndex).resource, 0, chunk.second);
				ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
				FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
				CheckResourceData(readbackBuffer, 0, currentDataStart, 0, chunk.second); // All memory might not be touched, ignore untouched memory
				currentDataStart += chunk.second;
			}

		}

		delete[] data;
		readbackBuffer->Release();
		fence->Release();
	};

	InitializationWrapper(func);
}