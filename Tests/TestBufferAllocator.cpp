#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/BufferAllocator.h"

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
		ID3D12Heap* heap = CreateResourceHeap(device,
			64 * 1024 * bufferMultiplier, D3D12_HEAP_TYPE_DEFAULT,
			D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS);
		if (heap == nullptr)
			FAIL() << "Cannot proceed with tests as a heap cannot be created";

		for (auto& info : bufferInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool mappable = !(allowedViews.rtv || allowedViews.uav);
				BufferAllocator bufferAllocator1;
				bufferAllocator1.Initialize(info, device, mappable, allowedViews,
					64 * 1024 * bufferMultiplier);

				BufferAllocator bufferAllocator2;
				bufferAllocator2.Initialize(info, device, false, allowedViews,
					heap, 0, 64 * 1024 * bufferMultiplier);

				ASSERT_EQ(info.alignment, bufferAllocator1.GetElementAlignment());
				ASSERT_EQ(info.alignment, bufferAllocator2.GetElementAlignment());
				ASSERT_EQ(info.elementSize, bufferAllocator1.GetElementSize());
				ASSERT_EQ(info.elementSize, bufferAllocator2.GetElementSize());
			}
		}
		heap->Release();
	}
	device->Release();
}

void MassAllocate(BufferAllocator& allocator, size_t nrOfElements)
{
	ASSERT_EQ(allocator.AllocateBuffer(1), 0);
	auto startHandle = allocator.GetHandle(0);
	ASSERT_EQ(startHandle.nrOfElements, 1);
	ASSERT_EQ(startHandle.startOffset, 0);

	for (size_t i = 1; i < nrOfElements; ++i)
	{
		ASSERT_EQ(allocator.AllocateBuffer(1), i);
		auto currentHandle = allocator.GetHandle(i);
		ASSERT_EQ(currentHandle.nrOfElements, 1);
		ASSERT_EQ(currentHandle.resource, startHandle.resource);
		ASSERT_EQ(currentHandle.startOffset, allocator.GetElementSize() * i);
	}
}
TEST(BufferAllocatorTest, AllocatesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

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
					64 * 1024 * bufferMultiplier);

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
		allocator.DeallocateBuffer(i);
}

TEST(BufferAllocatorTest, DeallocatesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

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
					64 * 1024 * bufferMultiplier);

				size_t nrOfAllocations = min(1000,
					(64 * 1024 * bufferMultiplier) / info.elementSize);
				MassAllocate(bufferAllocator, nrOfAllocations);
				MassDeallocate(bufferAllocator, nrOfAllocations);
				size_t expectedIndex = nrOfAllocations == 1000 ?
					nrOfAllocations : nrOfAllocations - 1;

				ASSERT_EQ(bufferAllocator.AllocateBuffer(1), 0);
				auto startHandle = bufferAllocator.GetHandle(0);
				ASSERT_EQ(startHandle.nrOfElements, 1);
				ASSERT_EQ(startHandle.startOffset, 0);
				for (size_t i = 0; i < nrOfAllocations - 1; ++i)
				{
					ASSERT_EQ(bufferAllocator.AllocateBuffer(1), expectedIndex);
					auto currentHandle = bufferAllocator.GetHandle(expectedIndex);
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
					64 * 1024 * bufferMultiplier);

				size_t nrOfAllocations = min(1000,
					(64 * 1024 * bufferMultiplier) / info.elementSize);
				MassAllocate(bufferAllocator, nrOfAllocations);
				size_t dataSize = nrOfAllocations * info.elementSize;
				unsigned char* data = new unsigned char[dataSize];

				for (size_t i = 0; i < nrOfAllocations; ++i)
				{
					memset(data + i * info.elementSize, i, info.elementSize);
					bufferAllocator.UpdateMappedBuffer(i,
						data + i * info.elementSize);
				}

				if(FAILED(commandStructure.allocator->Reset()))
					FAIL() << "Cannot proceed with tests since a command allocator could not be reset";

				if (FAILED(commandStructure.list->Reset(
					commandStructure.allocator, nullptr)))
				{
					FAIL() << "Cannot proceed with tests since a command allocator could not be reset";
				}

				commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
					bufferAllocator.GetHandle(0).resource, 0, dataSize);
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