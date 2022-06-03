#include "pch.h"

#include <array>
#include <utility>
#include <functional>

#include "../Neo Steelgear Graphics Core/FrameBufferComponent.h"

#include "D3D12Helper.h"

TEST(FrameBufferComponentTest, DefaultInitialisable)
{
	FrameBufferComponent<1> frameBufferComponent1;
	FrameBufferComponent<2> frameBufferComponent2;
	FrameBufferComponent<3> frameBufferComponent3;
	FrameBufferComponent<4> frameBufferComponent4;
	FrameBufferComponent<5> frameBufferComponent5;
}

void InitializationHelper(std::function<void(ID3D12Device*, 
	UpdateType, const BufferComponentInfo&, 
	std::vector<DescriptorAllocationInfo<BufferViewDesc>>,
	bool, unsigned int)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 4> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::MAP_UPDATE, UpdateType::COPY_UPDATE };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
		{true, false, false, false}, {false, true, false, false},
		{true, true, false, false} };

	std::array<unsigned int, 6> allocationSizes = { 1, 2, 10, 20, 100, 512};
	std::array<unsigned int, 6> allocationAmounts = { 1, 2, 10, 20, 50, 128};

	for (unsigned int bufferMultiplier = 1; bufferMultiplier < 4;
		bufferMultiplier++)
	{
		for (auto& updateType : updateTypes)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				for (auto& allocationSize : allocationSizes)
				{
					std::vector<BufferInfo> bufferInfos = { {1, allocationSize},
						{2, allocationSize}, {16, allocationSize},
						{4, allocationSize}, {64, allocationSize},
						{32, allocationSize}, {1024, allocationSize} };

					for (auto& bufferInfo : bufferInfos)
					{
						for (auto& nrOfAllocations : allocationAmounts)
						{
							size_t alignedSize = ((bufferInfo.elementSize +
								(bufferInfo.alignment - 1)) & ~(bufferInfo.alignment - 1));

							size_t heapSize = 64 * 1024 * bufferMultiplier;

							if (alignedSize * nrOfAllocations > heapSize)
							{
								continue; // We cannot fit all the buffers for this request
							}

							bool mapped = !(allowedViews.rtv || allowedViews.uav);

							if (!mapped && updateType == UpdateType::MAP_UPDATE)
								continue; // Not a possible combination

							mapped &= updateType == UpdateType::MAP_UPDATE;
							ResourceHeapInfo heapInfo(heapSize);
							BufferComponentInfo componentInfo = { bufferInfo,
								mapped, heapInfo };
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

							func(device, updateType, componentInfo,
								descriptorAllocationInfo, mapped, nrOfAllocations);
						}
					}
				}
			}
		}
	}

	device->Release();
}

template<FrameType frames>
void RuntimeInitialize(ID3D12Device* device, UpdateType updateType,
	const BufferComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
	bool mapped, unsigned int nrOfAllocations)
{
	(void)mapped;
	(void)nrOfAllocations;
	FrameBufferComponent<frames> bufferComponent;
	bufferComponent.Initialize(device, updateType, componentInfo,
		descriptorAllocationInfo);
}

TEST(FrameBufferComponentTest, RuntimeInitialisable)
{
	InitializationHelper(RuntimeInitialize<1>);
	InitializationHelper(RuntimeInitialize<2>);
	InitializationHelper(RuntimeInitialize<3>);
	InitializationHelper(RuntimeInitialize<4>);
}

template<FrameType frames>
void CreateSimpleBuffers(ID3D12Device* device, UpdateType updateType,
	const BufferComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
	bool mapped, unsigned int nrOfAllocations)
{
	(void)mapped;

	std::array<unsigned int, 4> elementsPerAllocation = { 1, 2, 10, 50 };

	for (auto& nrOfElements : elementsPerAllocation)
	{
		FrameBufferComponent<frames> bufferComponent;
		bufferComponent.Initialize(device, updateType, componentInfo,
			descriptorAllocationInfo);

		for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
		{
			ResourceIndex index = bufferComponent.CreateBuffer(nrOfElements);
			ASSERT_EQ(index, ResourceIndex(i));
		}

		size_t stride = ((componentInfo.bufferInfo.elementSize +
			(componentInfo.bufferInfo.alignment - 1)) & 
			~(componentInfo.bufferInfo.alignment - 1)) * nrOfElements;

		for (unsigned int frame = 0; frame < frames * 2; ++frame)
		{
			auto firstAdress = bufferComponent.GetVirtualAdress(ResourceIndex(0));
			
			for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
			{
				ResourceIndex index(i);
				auto currentAdress = bufferComponent.GetVirtualAdress(index);
				ASSERT_EQ(firstAdress + stride * i, currentAdress);
			}

			bufferComponent.SwapFrame();
		}
	}
}

TEST(FrameBufferComponentTest, CorrectlyCreatesSimpleBuffers)
{
	InitializationHelper(CreateSimpleBuffers<1>);
	InitializationHelper(CreateSimpleBuffers<2>);
	InitializationHelper(CreateSimpleBuffers<3>);
	InitializationHelper(CreateSimpleBuffers<4>);
}

template<FrameType frames>
void RemoveSimpleBuffers(ID3D12Device* device, UpdateType updateType,
	const BufferComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
	bool mapped, unsigned int nrOfAllocations)
{
	(void)mapped;

	std::array<unsigned int, 4> elementsPerAllocation = { 1, 2, 10, 50 };

	for (auto& nrOfElements : elementsPerAllocation)
	{
		FrameBufferComponent<frames> bufferComponent;
		bufferComponent.Initialize(device, updateType, componentInfo,
			descriptorAllocationInfo);

		for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
		{
			ResourceIndex index = bufferComponent.CreateBuffer(nrOfElements);
			ASSERT_EQ(index, ResourceIndex(i));
		}

		auto firstAdress = bufferComponent.GetVirtualAdress(ResourceIndex(0));

		for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
		{
			bufferComponent.RemoveComponent(ResourceIndex(i));
		}

		std::vector<ResourceIndex> indices;

		for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
		{
			indices.push_back(bufferComponent.CreateBuffer(nrOfElements));
			ASSERT_LE(indices.back(), ResourceIndex(nrOfAllocations / nrOfElements));
		}

		size_t stride = ((componentInfo.bufferInfo.elementSize +
			(componentInfo.bufferInfo.alignment - 1)) &
			~(componentInfo.bufferInfo.alignment - 1)) * nrOfElements;
		auto maxAdress = firstAdress + stride * (nrOfAllocations / nrOfElements);

		for (unsigned int frame = 0; frame < frames * 2; ++frame)
		{
			for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
			{
				auto currentAdress = bufferComponent.GetVirtualAdress(indices[i]);
				ASSERT_LE(currentAdress, maxAdress);
			}

			bufferComponent.SwapFrame();
		}
	}
}

TEST(FrameBufferComponentTest, CorrectlyRemovesSimpleBuffers)
{
	InitializationHelper(RemoveSimpleBuffers<1>);
	InitializationHelper(RemoveSimpleBuffers<2>);
	InitializationHelper(RemoveSimpleBuffers<3>);
	InitializationHelper(RemoveSimpleBuffers<4>);
}

template<FrameType frames>
void UpdateBuffers(ID3D12Device* device, UpdateType updateType,
	const BufferComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorAllocationInfo,
	bool mapped, unsigned int nrOfAllocations)
{
	(void)mapped;

	if (updateType == UpdateType::NONE)
		return;

	std::array<unsigned int, 4> elementsPerAllocation = { 1, 2, 10, 50 };
	size_t individualStride = ((componentInfo.bufferInfo.elementSize +
		(componentInfo.bufferInfo.alignment - 1)) &
		~(componentInfo.bufferInfo.alignment - 1));
	unsigned char* data = new unsigned char[nrOfAllocations * individualStride];

	SimpleCommandStructure commandStructure;
	bool result = CreateSimpleCommandStructure(commandStructure, device,
		D3D12_COMMAND_LIST_TYPE_COPY);
	if (result == false)
		throw "Cannot proceed with tests as command structure could not be created";

	ResourceUploader uploader;
	uploader.Initialize(device, nrOfAllocations * individualStride, AllocationStrategy::FIRST_FIT);
	UINT64 fenceValue = 0;
	ID3D12Fence* fence = CreateFence(device, fenceValue, D3D12_FENCE_FLAG_NONE);

	ID3D12Resource* readbackBuffer = CreateBuffer(device,
		individualStride * nrOfAllocations, true);
	if (readbackBuffer == nullptr)
		throw "Cannot proceed with tests as readback buffer cannot be created";

	for (auto& nrOfElements : elementsPerAllocation)
	{
		FrameBufferComponent<frames> bufferComponent;
		bufferComponent.Initialize(device, updateType, componentInfo,
			descriptorAllocationInfo);
		ZeroMemory(data, nrOfAllocations * individualStride);

		size_t stride = individualStride * nrOfElements;

		for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
		{
			memset(data + stride * i, static_cast<unsigned char>(i), 
				componentInfo.bufferInfo.elementSize);
			ResourceIndex index = bufferComponent.CreateBuffer(nrOfElements);
			ASSERT_EQ(index, ResourceIndex(i));
			bufferComponent.SetUpdateData(index, data + stride * i);
		}

		for (unsigned int frame = 0; frame < frames * 2; ++frame)
		{
			bufferComponent.PerformUpdates(commandStructure.list, uploader);
			ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
			FlushCommandQueue(fenceValue, commandStructure.queue, fence);
			uploader.RestoreUsedMemory();

			if (FAILED(commandStructure.allocator->Reset()))
				throw "Cannot proceed with tests as command allocator cannot be reset";

			if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
				throw "Cannot proceed with tests as command list cannot be reset";

			for (unsigned int i = 0; i < nrOfAllocations / nrOfElements; ++i)
			{
				ResourceIndex index(i);
				auto handle = bufferComponent.GetBufferHandle(index);
				commandStructure.list->CopyBufferRegion(readbackBuffer, i * stride,
					handle.resource, handle.startOffset, stride);
			}

			ExecuteGraphicsCommandList(commandStructure.list,
				commandStructure.queue);
			FlushCommandQueue(fenceValue, commandStructure.queue, fence);
			CheckResourceData(readbackBuffer, 0, data, 0,
				stride * (nrOfAllocations / nrOfElements));

			if (FAILED(commandStructure.allocator->Reset()))
				throw "Cannot proceed with tests as command allocator cannot be reset";

			if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
				throw "Cannot proceed with tests as command list cannot be reset";

			bufferComponent.SwapFrame();
		}
	}

	delete[] data;
	readbackBuffer->Release();
	fence->Release();
}

TEST(FrameBufferComponentTest, CorrectlyUpdatesBuffers)
{
	InitializationHelper(UpdateBuffers<1>);
	InitializationHelper(UpdateBuffers<2>);
	InitializationHelper(UpdateBuffers<3>);
	InitializationHelper(UpdateBuffers<4>);
}