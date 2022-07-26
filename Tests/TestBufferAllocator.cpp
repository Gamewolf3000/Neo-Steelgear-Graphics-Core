#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/BufferAllocator.h"
#include "../Neo Steelgear Graphics Core/MultiHeapAllocatorGPU.h"

#include "D3D12Helper.h"

TEST(BufferAllocatorTest, DefaultInitialisable)
{
	BufferAllocator bufferAllocator;
}

TEST(BufferAllocatorTest, RuntimeInitialisable)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	std::vector<BufferInfo> bufferInfos = { {1, 1}, {1, 32}, {2, 2}, {16, 16},
		{4, 8}, {64, 64}, {32, 128}, {2, 256}, {4, 4}, {8, 32}, {1024, 4096} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false}, 
		{true, false, false, false}, {false, true, false, false},
		{false, false, true, false}, {true, true, false, false},
		{true, false, true, false}, {false, true, true, false},
		{true, true, true, false}};

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 11;
		bufferMultiplier++)
	{
		for (auto& info : bufferInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool mappable = !(allowedViews.rtv || allowedViews.uav);
				BufferAllocator bufferAllocator;
				bufferAllocator.Initialize(info, device, mappable, allowedViews,
					64 * 1024 * bufferMultiplier, 0, &heapAllocator);

				ASSERT_EQ(info.alignment, bufferAllocator.GetElementAlignment());
				ASSERT_EQ(info.elementSize, bufferAllocator.GetElementSize());
			}
		}
	}
	device->Release();
}

void MassAllocate(BufferAllocator& allocator, size_t nrOfElements)
{
	auto firstIdentifier = allocator.AllocateBuffer(1);
	ASSERT_EQ(firstIdentifier.heapChunkIndex, 0);
	ASSERT_EQ(firstIdentifier.internalIndex, 0);
	auto startHandle = allocator.GetHandle(firstIdentifier);
	ASSERT_EQ(startHandle.nrOfElements, 1);
	ASSERT_EQ(startHandle.startOffset, 0);

	size_t expectedOffset = allocator.GetElementSize();
	size_t expectedInternalIndex = 1;
	size_t currentChunkIndex = 0;

	for (size_t i = 1; i < nrOfElements; ++i)
	{
		auto currentIdentifier = allocator.AllocateBuffer(1);

		if (currentIdentifier.heapChunkIndex != currentChunkIndex)
		{
			expectedOffset = 0;
			expectedInternalIndex = 0;
			currentChunkIndex = currentIdentifier.heapChunkIndex;
			startHandle = allocator.GetHandle(currentIdentifier);
		}

		ASSERT_EQ(currentIdentifier.internalIndex, expectedInternalIndex);
		auto currentHandle = allocator.GetHandle(currentIdentifier);
		ASSERT_EQ(currentHandle.nrOfElements, 1);
		ASSERT_EQ(currentHandle.resource, startHandle.resource);
		ASSERT_EQ(currentHandle.startOffset, expectedOffset);

		expectedOffset += allocator.GetElementSize();
		++expectedInternalIndex;
	}
}
TEST(BufferAllocatorTest, AllocatesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	std::vector<BufferInfo> bufferInfos = { {1, 1}, {1, 32}, {2, 2}, {16, 16},
	{4, 8}, {64, 64}, {32, 128}, {2, 256}, {64, 64 * 1024}, {8, 32}, {1024, 4096} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
	{true, false, false, false}, {false, true, false, false},
	{false, false, true, false}, {true, true, false, false},
	{true, false, true, false}, {false, true, true, false},
	{true, true, true, false} };

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 11;
		bufferMultiplier++)
	{
		for (auto& info : bufferInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool mappable = !(allowedViews.rtv || allowedViews.uav);
				BufferAllocator bufferAllocator;
				bufferAllocator.Initialize(info, device, mappable, allowedViews,
					64 * 1024 * bufferMultiplier, 0, &heapAllocator);

				size_t nrOfAllocations = min(1000, 
					(64 * 1024 * bufferMultiplier) / info.elementSize);
				MassAllocate(bufferAllocator, nrOfAllocations);
			}
		}
	}
	device->Release();
}

void MassDeallocate(BufferAllocator& allocator, size_t nrOfElements)
{
	for (size_t i = 0; i < nrOfElements; ++i)
	{
		ResourceIdentifier identifier;
		identifier.heapChunkIndex = 0;
		identifier.internalIndex = i;
		allocator.DeallocateBuffer(identifier);
	}
}

TEST(BufferAllocatorTest, DeallocatesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	std::vector<BufferInfo> bufferInfos = { {1, 1}, {1, 32}, {64, 64}, {2, 256},
		{64, 64 * 1024}, {1024, 4096} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
	{true, false, false, false}, {false, true, false, false},
	{false, false, true, false}, {true, true, false, false},
	{true, false, true, false}, {false, true, true, false},
	{true, true, true, false} };

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 11;
		bufferMultiplier++)
	{
		for (auto& info : bufferInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool mappable = !(allowedViews.rtv || allowedViews.uav);
				BufferAllocator bufferAllocator;
				bufferAllocator.Initialize(info, device, mappable, allowedViews,
					64 * 1024 * bufferMultiplier, 0, &heapAllocator);

				size_t nrOfAllocations = min(1000,
					(64 * 1024 * bufferMultiplier) / info.elementSize);
				MassAllocate(bufferAllocator, nrOfAllocations);
				MassDeallocate(bufferAllocator, nrOfAllocations);
				size_t expectedIndex = nrOfAllocations == 1000 ?
					nrOfAllocations : nrOfAllocations - 1;

				auto firstNewIdentifier = bufferAllocator.AllocateBuffer(1);
				ASSERT_EQ(firstNewIdentifier.heapChunkIndex, 0);
				ASSERT_EQ(firstNewIdentifier.internalIndex, 0);
				auto startHandle = bufferAllocator.GetHandle(firstNewIdentifier);
				ASSERT_EQ(startHandle.nrOfElements, 1);
				ASSERT_EQ(startHandle.startOffset, 0);
				for (size_t i = 0; i < nrOfAllocations - 1; ++i)
				{
					auto currentIdentifier = bufferAllocator.AllocateBuffer(1);
					ASSERT_EQ(currentIdentifier.heapChunkIndex, 0);
					ASSERT_EQ(currentIdentifier.internalIndex, expectedIndex);
					auto currentHandle = bufferAllocator.GetHandle(currentIdentifier);
					ASSERT_EQ(currentHandle.nrOfElements, 1);
					ASSERT_EQ(currentHandle.resource, startHandle.resource);
					ASSERT_EQ(currentHandle.startOffset,
						bufferAllocator.GetElementSize() * (i + 1));
					expectedIndex -= 1;
				}
			}
		}
	}
	device->Release();
}

TEST(BufferAllocatorTest, UpdatesMappedBuffersCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device))
		FAIL() << "Cannot proceed with tests as command structure cannot be created";

	commandStructure.list->Close();
	ID3D12Resource* readbackBuffer = CreateBuffer(device, 64 * 1024 * 11, true);
	if (readbackBuffer == nullptr)
		FAIL() << "Cannot proceed with tests as a resource could not be created";

	size_t currentFenceValue = 0;
	ID3D12Fence* fence = CreateFence(device, currentFenceValue, 
		D3D12_FENCE_FLAG_NONE);
	if (fence == nullptr)
		FAIL() << "Cannot proceed with tests as a fence could not be created";

	std::vector<BufferInfo> bufferInfos = { {1, 1}, {1, 32}, {64, 64}, {2, 256},
		{64, 64 * 1024}, {1024, 4096} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
	{true, false, false, false}};

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 11;
		bufferMultiplier++)
	{
		for (auto& info : bufferInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool mappable = !(allowedViews.rtv || allowedViews.uav);
				BufferAllocator bufferAllocator;
				bufferAllocator.Initialize(info, device, mappable, allowedViews,
					64 * 1024 * bufferMultiplier, 0, &heapAllocator);

				size_t nrOfAllocations = min(1000,
					(64 * 1024 * bufferMultiplier) / info.elementSize);
				MassAllocate(bufferAllocator, nrOfAllocations);
				size_t dataSize = nrOfAllocations * info.elementSize;
				unsigned char* data = new unsigned char[dataSize];

				for (size_t i = 0; i < nrOfAllocations; ++i)
				{
					ResourceIdentifier identifier;
					identifier.heapChunkIndex = 0;
					identifier.internalIndex = i;
					memset(data + i * info.elementSize, i, info.elementSize);
					bufferAllocator.UpdateMappedBuffer(identifier,
						data + i * info.elementSize);
				}

				if(FAILED(commandStructure.allocator->Reset()))
					FAIL() << "Cannot proceed with tests since a command allocator could not be reset";

				if (FAILED(commandStructure.list->Reset(
					commandStructure.allocator, nullptr)))
				{
					FAIL() << "Cannot proceed with tests since a command allocator could not be reset";
				}

				ResourceIdentifier startIdentifier;
				startIdentifier.heapChunkIndex = 0;
				startIdentifier.internalIndex = 0;
				commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
					bufferAllocator.GetHandle(startIdentifier).resource, 0, dataSize);
				ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
				FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
				CheckResourceData(readbackBuffer, 0, data, 0, dataSize);

				delete[] data;
			}
		}
	}
	readbackBuffer->Release();
	fence->Release();
	device->Release();
}

TEST(BufferAllocatorTest, ExpandsCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device))
		FAIL() << "Cannot proceed with tests as command structure cannot be created";

	commandStructure.list->Close();

	ID3D12Resource* readbackBuffer = CreateBuffer(device, 64 * 1024 * 11, true);
	if (readbackBuffer == nullptr)
		FAIL() << "Cannot proceed with tests as a resource could not be created";

	size_t currentFenceValue = 0;
	ID3D12Fence* fence = CreateFence(device, currentFenceValue,
		D3D12_FENCE_FLAG_NONE);
	if (fence == nullptr)
		FAIL() << "Cannot proceed with tests as a fence could not be created";

	std::vector<BufferInfo> bufferInfos = { {64, 64}, {2, 256},
		{64, 4096}, {1024, 4096} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
	{true, false, false, false} };

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 5;
		bufferMultiplier++)
	{
		for (auto& info : bufferInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool mappable = !(allowedViews.rtv || allowedViews.uav);
				BufferAllocator bufferAllocator;
				bufferAllocator.Initialize(info, device, mappable, allowedViews,
					4096, 4096, &heapAllocator);

				size_t nrOfAllocations = min(1000,
					(64 * 1024 * bufferMultiplier * 2) / info.elementSize);
				MassAllocate(bufferAllocator, nrOfAllocations);
				size_t dataSize = nrOfAllocations * info.elementSize;
				unsigned char* data = new unsigned char[dataSize];

				for (size_t i = 0; i < nrOfAllocations; ++i)
				{
					ResourceIdentifier identifier;
					identifier.heapChunkIndex = i * info.elementSize / 4096;
					identifier.internalIndex = i - (4096 / info.elementSize) *
						identifier.heapChunkIndex;
					memset(data + i * info.elementSize, i, info.elementSize);
					bufferAllocator.UpdateMappedBuffer(identifier,
						data + i * info.elementSize);
				}

				unsigned char* currentDataPtr = data;
				size_t remainingData = dataSize;
				size_t nrOfChunks = std::ceil(nrOfAllocations * info.elementSize / 4096.0);
				for (size_t chunkIndex = 0; chunkIndex < nrOfChunks; ++chunkIndex)
				{
					if (FAILED(commandStructure.allocator->Reset()))
						FAIL() << "Cannot proceed with tests since a command allocator could not be reset";

					if (FAILED(commandStructure.list->Reset(
						commandStructure.allocator, nullptr)))
					{
						FAIL() << "Cannot proceed with tests since a command allocator could not be reset";
					}

					size_t currentDataSize = std::min<size_t>(remainingData, 4096);
					ResourceIdentifier startIdentifier;
					startIdentifier.heapChunkIndex = chunkIndex;
					startIdentifier.internalIndex = 0;
					commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
						bufferAllocator.GetHandle(startIdentifier).resource, 0, currentDataSize);
					ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
					FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
					CheckResourceData(readbackBuffer, 0, currentDataPtr, 0, currentDataSize);
					currentDataPtr += currentDataSize;
					remainingData -= currentDataSize;
				}

				delete[] data;
			}
		}
	}
	readbackBuffer->Release();
	fence->Release();
	device->Release();
}