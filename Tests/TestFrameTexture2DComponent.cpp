#include "pch.h"

#include <array>
#include <utility>
#include <functional>

#include "../Neo Steelgear Graphics Core/FrameTexture2DComponent.h"

#include "D3D12Helper.h"

TEST(FrameTexture2DComponentTest, DefaultInitialisable)
{
	FrameTexture2DComponent<1> frameTexture2DComponent1;
	FrameTexture2DComponent<2> frameTexture2DComponent2;
	FrameTexture2DComponent<3> frameTexture2DComponent3;
}

void InitializationHelper(std::function<void(ID3D12Device*, UpdateType,
	const TextureComponentInfo&,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 3> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::COPY_UPDATE };

	std::array<size_t, 2> heapSizes = { 100000000, 1000000000 };

	std::vector<TextureInfo> textureInfos = { {DXGI_FORMAT_R8G8B8A8_UNORM, 4},
		{DXGI_FORMAT_R32G32B32A32_FLOAT, 16} };

	std::vector<AllowedViews> ViewCombinations = { 
		{true, false, false, false}, {true, true, true, false}};

	for (auto& heapSize : heapSizes)
	{
		for (auto& textureInfo : textureInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				for (auto& updateType : updateTypes)
				{
					bool unacceptableCombination = false;
					unacceptableCombination |=
						textureInfo.format != DXGI_FORMAT_D32_FLOAT &&
						allowedViews.dsv;
					unacceptableCombination |=
						textureInfo.format == DXGI_FORMAT_D32_FLOAT &&
						!allowedViews.dsv;
					unacceptableCombination |=
						textureInfo.format == DXGI_FORMAT_R32G32B32_UINT &&
						(allowedViews.rtv || allowedViews.uav);

					if (unacceptableCombination)
						continue;

					size_t maxNrOfTextures = heapSize / 10000;
					std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>
						descriptorAllocationInfo =
						CreateDescriptorAllocationInfo(maxNrOfTextures, allowedViews);

					TextureComponentInfo componentInfo(textureInfo.format,
						textureInfo.texelSize, ResourceHeapInfo(heapSize));

					func(device, updateType, componentInfo,
						descriptorAllocationInfo);
				}
			}
		}
	}

	device->Release();
}

template<FrameType frames>
void RuntimeInitialize(ID3D12Device* device, UpdateType updateType,
	const TextureComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>& descriptorAllocationInfo)
{
	FrameTexture2DComponent<frames> texture2DComponent;
	texture2DComponent.Initialize(device, updateType, componentInfo,
		descriptorAllocationInfo);
}

TEST(FrameTexture2DComponentTest, RuntimeInitialisable)
{
	InitializationHelper(RuntimeInitialize<1>);
	InitializationHelper(RuntimeInitialize<2>);
	InitializationHelper(RuntimeInitialize<3>);
}

template<FrameType frames>
void CreateTextures(ID3D12Device* device, UpdateType updateType,
	const TextureComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>& descriptorAllocationInfo)
{
	std::array<UINT, 4> dimensions = { 1, 2, 499, 1024 };
	std::array<UINT16, 3> arraySizes = { 1, 2, 5 };
	std::array<UINT16, 2> mipLevels = { 1, 0 };

	for (auto width : dimensions)
	{
		size_t currentlyUsedMemory = 0;
		size_t currentExpectedIndex = 0;
		FrameTexture2DComponent<frames> textureComponent;
		textureComponent.Initialize(device, updateType, componentInfo,
			descriptorAllocationInfo);

		for (auto height : dimensions)
		{
			for (auto mips : mipLevels)
			{
				for (auto arraySize : arraySizes)
				{
					AllowedViews allowedViews =
						ReverseDescriptorAllocationInfo(descriptorAllocationInfo);
					D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width, height,
						mips, arraySize, componentInfo.textureInfo.format, allowedViews);
					D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
						device->GetResourceAllocationInfo(0, 1, &desc);

					size_t alignedStart = ((currentlyUsedMemory +
						(allocationInfo.Alignment - 1)) &
						~(allocationInfo.Alignment - 1));
					size_t alignedEnd = alignedStart +
						allocationInfo.SizeInBytes;

					if (alignedEnd > componentInfo.heapInfo.info.owned.heapSize)
						break;

					currentlyUsedMemory = alignedEnd;
					TextureAllocationInfo textureAllocationInfo(width, height,
						arraySize, mips);
					ResourceIndex index = textureComponent.CreateTexture(
						textureAllocationInfo);
					ASSERT_EQ(index, currentExpectedIndex++);
				}
			}
		}

		for (unsigned int frame = 0; frame < frames + 1; ++frame)
			textureComponent.SwapFrame();
	}
}

TEST(FrameTexture2DComponentTest, CreatesTexturesCorrectly)
{
	InitializationHelper(CreateTextures<1>);
	InitializationHelper(CreateTextures<2>);
	InitializationHelper(CreateTextures<3>);
}

template<FrameType frames>
void RemoveTextures(ID3D12Device* device, UpdateType updateType,
	const TextureComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>& descriptorAllocationInfo)
{
	std::array<UINT, 4> dimensions = { 1, 2, 499, 1024 };
	std::array<UINT16, 3> arraySizes = { 1, 2, 5 };
	std::array<UINT16, 2> mipLevels = { 1, 0 };
	std::vector<std::pair<ResourceIndex, TextureAllocationInfo>> allocations;

	for (auto width : dimensions)
	{
		size_t currentlyUsedMemory = 0;
		size_t currentExpectedIndex = 0;
		FrameTexture2DComponent<frames> textureComponent;
		textureComponent.Initialize(device, updateType, componentInfo,
			descriptorAllocationInfo);

		for (auto height : dimensions)
		{
			for (auto mips : mipLevels)
			{
				for (auto arraySize : arraySizes)
				{
					AllowedViews allowedViews =
						ReverseDescriptorAllocationInfo(descriptorAllocationInfo);
					D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width, height,
						mips, arraySize, componentInfo.textureInfo.format, allowedViews);
					D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
						device->GetResourceAllocationInfo(0, 1, &desc);

					size_t alignedStart = ((currentlyUsedMemory +
						(allocationInfo.Alignment - 1)) &
						~(allocationInfo.Alignment - 1));
					size_t alignedEnd = alignedStart +
						allocationInfo.SizeInBytes;

					if (alignedEnd > componentInfo.heapInfo.info.owned.heapSize)
						break;

					currentlyUsedMemory = alignedEnd;
					TextureAllocationInfo textureAllocationInfo(width, height,
						arraySize, mips);
					ResourceIndex index = textureComponent.CreateTexture(
						textureAllocationInfo);
					ASSERT_EQ(index, currentExpectedIndex++);
					allocations.push_back(std::make_pair(index, 
						textureAllocationInfo));
				}
			}
		}

		for (auto& allocation : allocations)
			textureComponent.RemoveComponent(allocation.first);

		for (auto& allocation : allocations)
		{
			ResourceIndex index = textureComponent.CreateTexture(allocation.second);
			ASSERT_LE(index, ResourceIndex(allocations.size()));
		}

		for (unsigned int frame = 0; frame < frames + 1; ++frame)
			textureComponent.SwapFrame();

		allocations.clear();
	}
}

TEST(FrameTexture2DComponentTest, RemovesTexturesCorrectly)
{
	InitializationHelper(RemoveTextures<1>);
	InitializationHelper(RemoveTextures<2>);
	InitializationHelper(RemoveTextures<3>);
}

template<FrameType frames>
void UpdateTextures(ID3D12Device* device, UpdateType updateType,
	const TextureComponentInfo& componentInfo,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>& descriptorAllocationInfo)
{
	if (updateType == UpdateType::NONE)
		return;

	std::array<UINT, 4> dimensions = { 1, 2, 499, 1024 };
	std::array<UINT16, 3> arraySizes = { 1, 2, 5 };
	std::array<UINT16, 2> mipLevels = { 1, 0 };
	std::vector<std::pair<ResourceIndex, TextureAllocationInfo>> allocations;

	size_t currentFenceValue = 0;
	ID3D12Fence* fence = CreateFence(device, currentFenceValue,
		D3D12_FENCE_FLAG_NONE);

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device,
		D3D12_COMMAND_LIST_TYPE_COPY))
	{
		throw "Cannot proceed with tests as command structure cannot be created";
	}

	commandStructure.list->Close();

	size_t heapSize = componentInfo.heapInfo.info.owned.heapSize;
	ResourceUploader uploader;
	uploader.Initialize(device, 2 * heapSize, AllocationStrategy::FIRST_FIT);

	unsigned char* data = new unsigned char[heapSize];

	for (auto width : dimensions)
	{
		size_t currentlyUsedMemory = 0;
		size_t currentDataOffset = 0;
		size_t totalAdded = 0;
		FrameTexture2DComponent<frames> component;
		component.Initialize(device, updateType, componentInfo, {}); // Views should not affect this

		for (auto height : dimensions)
		{
			for (auto mips : mipLevels)
			{
				for (auto arraySize : arraySizes)
				{
					D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width,
						height, mips, arraySize, componentInfo.textureInfo.format,
						{ false, false, false, false });
					D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
						device->GetResourceAllocationInfo(0, 1, &desc);

					size_t alignedStart = ((currentlyUsedMemory +
						(allocationInfo.Alignment - 1)) &
						~(allocationInfo.Alignment - 1));
					size_t alignedEnd = alignedStart +
						allocationInfo.SizeInBytes;

					if (alignedEnd > heapSize)
						break;

					currentlyUsedMemory = alignedEnd;

					TextureAllocationInfo textureAllocationInfo(width,
						height, arraySize, mips);
					auto index = component.CreateTexture(textureAllocationInfo);
					ASSERT_NE(index, ResourceIndex(-1));
					allocations.push_back(std::make_pair(index,
						textureAllocationInfo));
					auto handle = component.GetTextureHandle(index);

					size_t textureByteSize = 0;
					unsigned int subresources =
						handle.resource->GetDesc().MipLevels * arraySize;
					unsigned int internalOffset = 0;

					for (unsigned int subresource = 0;
						subresource < subresources; ++subresource)
					{
						D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedFootprint;
						UINT nrOfRows = 0;
						UINT64 rowSize = 0, subresourceSize = 0;
						device->GetCopyableFootprints(&desc, subresource,
							1, 0, &placedFootprint, &nrOfRows,
							&rowSize, &subresourceSize);
						textureByteSize += nrOfRows * rowSize;

						unsigned char* currentData = data + currentDataOffset
							+ internalOffset;
						memset(currentData,
							static_cast<unsigned char>(subresource + 1),
							nrOfRows * rowSize);
						component.SetUpdateData(index, currentData,
							subresource);
						internalOffset += nrOfRows * rowSize;
					}

					currentDataOffset += textureByteSize;
					++totalAdded;
				}
			}
		}

		currentDataOffset = 0;
		for (unsigned int i = 0; i < allocations.size() / 2; ++i)
		{
			component.RemoveComponent(allocations[i].first);
			allocations[i].first = component.CreateTexture(allocations[i].second);
			ASSERT_NE(allocations[i].first, ResourceIndex(-1));
			auto handle = component.GetTextureHandle(allocations[i].first);
			auto desc = handle.resource->GetDesc();
			size_t textureByteSize = 0;
			unsigned int subresources = desc.MipLevels * desc.DepthOrArraySize;
			unsigned int internalOffset = 0;

			for (unsigned int subresource = 0; subresource < subresources; ++subresource)
			{
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedFootprint;
				UINT nrOfRows = 0;
				UINT64 rowSize = 0, subresourceSize = 0;
				device->GetCopyableFootprints(&desc, subresource,
					1, 0, &placedFootprint, &nrOfRows,
					&rowSize, &subresourceSize);
				textureByteSize += nrOfRows * rowSize;

				unsigned char* currentData = data + currentDataOffset
					+ internalOffset;
				memset(currentData,
					static_cast<unsigned char>(subresource + 1),
					nrOfRows * rowSize);
				component.SetUpdateData(allocations[i].first, currentData, subresource);
				internalOffset += nrOfRows * rowSize;
			}

			currentDataOffset += textureByteSize;
		}

		for (unsigned int currentFrame = 0; currentFrame < frames + 1;
			++currentFrame)
		{
			ID3D12Resource* readbackBuffer = CreateBuffer(device, heapSize, true);
			if (readbackBuffer == nullptr)
				throw "Cannot proceed with tests as readback buffer cannot be created";

			if (FAILED(commandStructure.allocator->Reset()))
				throw "Cannot proceed with tests as command allocator cannot be reset";

			if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
				throw "Cannot proceed with tests as command list cannot be reset";

			std::vector<D3D12_RESOURCE_BARRIER> barriers;
			component.PrepareResourcesForUpdates(barriers);
			component.PerformUpdates(commandStructure.list, uploader);

			ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
			FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
			size_t totalChecked = 0;

			for (size_t i = 0; i < allocations.size(); ++i)
			{
				if (FAILED(commandStructure.allocator->Reset()))
					throw "Cannot proceed with tests as command allocator cannot be reset";

				if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
					throw "Cannot proceed with tests as command list cannot be reset";

				ResourceIndex index(allocations[i].first);
				auto handle = component.GetTextureHandle(index);
				auto desc = handle.resource->GetDesc();
				unsigned int subresources = desc.MipLevels * desc.DepthOrArraySize;
				unsigned int internalOffset = 0;

				D3D12_TEXTURE_COPY_LOCATION source;
				source.pResource = handle.resource;
				source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

				D3D12_TEXTURE_COPY_LOCATION destination;
				destination.pResource = readbackBuffer;
				destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

				std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT>
					footprints(subresources);
				std::vector<UINT> rows(subresources);
				std::vector<UINT64> rowSizes(subresources);
				device->GetCopyableFootprints(&desc, 0, subresources, 0,
					footprints.data(), rows.data(), rowSizes.data(), nullptr);

				for (unsigned int subresource = 0; subresource < subresources;
					++subresource)
				{
					source.SubresourceIndex = subresource;
					destination.PlacedFootprint = footprints[subresource];

					commandStructure.list->CopyTextureRegion(&destination,
						0, 0, 0, &source, nullptr);

					internalOffset += rows[subresource] * rowSizes[subresource];
				}

				ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
				FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
				CheckTextureData(readbackBuffer, footprints, rows, rowSizes,
					data + totalChecked);
				totalChecked += internalOffset;
			}

			uploader.RestoreUsedMemory();
			readbackBuffer->Release();
			component.SwapFrame();
			allocations.clear();
		}
	}

	fence->Release();
	delete[] data;
}

TEST(FrameTexture2DComponentTest, UpdatesTexturesCorrectly)
{
	InitializationHelper(UpdateTextures<1>);
	InitializationHelper(UpdateTextures<2>);
	InitializationHelper(UpdateTextures<3>);
}