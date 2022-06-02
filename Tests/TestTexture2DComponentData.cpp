#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/Texture2DComponentData.h"

#include "D3D12Helper.h"

TEST(Texture2DComponentDataTest, DefaultInitialisable)
{
	Texture2DComponentData componentData;
}

void InitializationHelper(std::function<void(ID3D12Device*, unsigned int,
	UpdateType, unsigned int)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<UpdateType, 4> updateTypes = { UpdateType::NONE,
	UpdateType::INITIALISE_ONLY, UpdateType::COPY_UPDATE };

	for (unsigned int frame = 1; frame < 9; ++frame)
	{
		for (auto& updateType : updateTypes)
		{
			for (unsigned int sizeExponent = 6; sizeExponent < 9; ++sizeExponent)
			{
				func(device, frame, updateType, pow(10, sizeExponent));
			}
		}
	}

	device->Release();
}

TEST(Texture2DComponentDataTest, RuntimeInitialisable)
{
	auto lambda = [](ID3D12Device* device, unsigned int totalNrOfFrames,
		UpdateType updateType, unsigned int totalSize)
	{
		Texture2DComponentData componentData;
		componentData.Initialize(device, totalNrOfFrames, updateType, totalSize);
	};

	InitializationHelper(lambda);
}

TEST(Texture2DComponentDataTest, PerformsSimpleAddsCorrectly)
{
	auto lambda = [](ID3D12Device* device, unsigned int totalNrOfFrames,
		UpdateType updateType, unsigned int totalSize)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };
		std::vector<std::pair<DXGI_FORMAT, std::uint8_t>> textureInfos = {
			{DXGI_FORMAT_R8G8B8A8_UNORM, 4}, {DXGI_FORMAT_D32_FLOAT, 4},
			{DXGI_FORMAT_R32G32B32A32_FLOAT, 16}, {DXGI_FORMAT_R32G32B32_UINT, 12} };

		for (auto width : dimensions)
		{
			size_t currentlyUsedMemory = 0;
			size_t currentDataOffset = 0;
			size_t currentExpectedIndex = 0;
			std::vector<std::pair<ID3D12Resource*, size_t>> textures;
			Texture2DComponentData componentData;
			componentData.Initialize(device, totalNrOfFrames, updateType,
				totalSize);

			for (auto height : dimensions)
			{
				for (auto mips : mipLevels)
				{
					for (auto arraySize : arraySizes)
					{
						for (auto textureInfo : textureInfos)
						{
							D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width,
								height, mips, arraySize, textureInfo.first,
								{ false, false, false, false });
							D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
								device->GetResourceAllocationInfo(0, 1, &desc);

							size_t alignedStart = ((currentlyUsedMemory +
								(allocationInfo.Alignment - 1)) &
								~(allocationInfo.Alignment - 1));
							size_t alignedEnd = alignedStart +
								allocationInfo.SizeInBytes;

							if (alignedEnd > totalSize)
								break;

							currentlyUsedMemory = alignedEnd;
							auto resource = CreateTexture2D(device, desc);
							ResourceIndex index(currentExpectedIndex);
							size_t textureByteSize = 0;

							unsigned int subresources = 
								resource->GetDesc().MipLevels * arraySize;

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
							}

							componentData.AddComponent(index, textureByteSize,
								resource);
							textures.push_back(std::make_pair(resource,
								textureByteSize));
							
							if (updateType != UpdateType::NONE)
							{
								unsigned char* startPtr = 
									static_cast<unsigned char*>(
									componentData.GetComponentData(
										ResourceIndex(0)));
								void* current =
									componentData.GetComponentData(index);
								ASSERT_EQ(startPtr + currentDataOffset, current);
							}
							
							currentDataOffset += textureByteSize;
							++currentExpectedIndex;
						}
					}
				}
			}

			for (auto& texture : textures)
				texture.first->Release();
		}
	};

	InitializationHelper(lambda);
}

TEST(Texture2DComponentDataTest, PerformsSimpleRemovesCorrectly)
{
	auto lambda = [](ID3D12Device* device, unsigned int totalNrOfFrames,
		UpdateType updateType, unsigned int totalSize)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };
		std::vector<std::pair<DXGI_FORMAT, std::uint8_t>> textureInfos = {
			{DXGI_FORMAT_R8G8B8A8_UNORM, 4}, {DXGI_FORMAT_D32_FLOAT, 4}, 
			{DXGI_FORMAT_R32G32B32A32_FLOAT, 16}, 
			{DXGI_FORMAT_R32G32B32_UINT, 12} };

		for (auto width : dimensions)
		{
			size_t currentlyUsedMemory = 0;
			size_t currentDataOffset = 0;
			size_t currentExpectedIndex = 0;
			std::vector<std::pair<ID3D12Resource*, size_t>> textures;
			Texture2DComponentData componentData;
			componentData.Initialize(device, totalNrOfFrames, updateType,
				totalSize);

			for (auto height : dimensions)
			{
				for (auto mips : mipLevels)
				{
					for (auto arraySize : arraySizes)
					{
						for (auto textureInfo : textureInfos)
						{
							D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width,
								height, mips, arraySize, textureInfo.first,
								{ false, false, false, false });
							D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
								device->GetResourceAllocationInfo(0, 1, &desc);

							size_t alignedStart = ((currentlyUsedMemory +
								(allocationInfo.Alignment - 1)) &
								~(allocationInfo.Alignment - 1));
							size_t alignedEnd = alignedStart +
								allocationInfo.SizeInBytes;

							if (alignedEnd > totalSize)
								break;

							currentlyUsedMemory = alignedEnd;
							auto resource = CreateTexture2D(device, desc);
							ResourceIndex index(currentExpectedIndex);
							size_t textureByteSize = 0;

							unsigned int subresources =
								resource->GetDesc().MipLevels * arraySize;

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
							}

							componentData.AddComponent(index, textureByteSize,
								resource);
							textures.push_back(std::make_pair(resource,
								textureByteSize));

							if (updateType != UpdateType::NONE)
							{
								unsigned char* startPtr =
									static_cast<unsigned char*>(
										componentData.GetComponentData(
											ResourceIndex(0)));
								void* current =
									componentData.GetComponentData(index);
								ASSERT_EQ(startPtr + currentDataOffset, current);
							}

							currentDataOffset += textureByteSize;
							++currentExpectedIndex;
						}
					}
				}
			}

			unsigned char* dataStart = static_cast<unsigned char*>(
				componentData.GetComponentData(ResourceIndex(0)));

			for (unsigned int indexCounter = 0; indexCounter < textures.size();
				++indexCounter)
			{
				componentData.RemoveComponent(ResourceIndex(indexCounter));
			}

			for (unsigned int indexCounter = 0; 
				indexCounter < textures.size(); ++indexCounter)
			{
				auto& entry = textures[indexCounter];
				auto desc = entry.first->GetDesc();
				auto index = ResourceIndex(indexCounter);

				componentData.AddComponent(index, entry.second, entry.first);

				unsigned char* currentStart = static_cast<unsigned char*>(
					componentData.GetComponentData(index));

				ASSERT_LE(currentStart + entry.second, dataStart + currentDataOffset);
			}

			for (auto& texture : textures)
				texture.first->Release();
		}
	};

	InitializationHelper(lambda);
}

TEST(Texture2DComponentDataTest, PerformsUpdatesCorrectly)
{
	auto lambda = [](ID3D12Device* device, unsigned int totalNrOfFrames,
		UpdateType updateType, unsigned int totalSize)
	{
		if (updateType == UpdateType::NONE)
			return;

		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };
		std::vector<std::pair<DXGI_FORMAT, std::uint8_t>> textureInfos = { 
			{DXGI_FORMAT_R8G8B8A8_UNORM, 4}, {DXGI_FORMAT_D32_FLOAT, 4},
			{DXGI_FORMAT_R32G32B32A32_FLOAT, 16}, {DXGI_FORMAT_R32G32B32_UINT, 12} };

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

		ResourceUploader uploader;
		uploader.Initialize(device, 2 * totalSize, AllocationStrategy::FIRST_FIT);

		unsigned char* data = new unsigned char[totalSize];

		for (auto textureInfo : textureInfos)
		{
			size_t currentlyUsedMemory = 0;
			size_t currentDataOffset = 0;
			size_t totalAdded = 0;
			Texture2DComponentData componentData;
			componentData.Initialize(device, totalNrOfFrames, updateType,
				totalSize);

			TextureComponentInfo componentInfo(textureInfo.first,
				textureInfo.second, ResourceHeapInfo(totalSize));
			Texture2DComponent component;
			component.Initialize(device, componentInfo, {}); // Views should not affect this

			for (auto width : dimensions)
			{
				for (auto height : dimensions)
				{
					for (auto mips : mipLevels)
					{
						for (auto arraySize : arraySizes)
						{
							D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width,
								height, mips, arraySize, textureInfo.first,
								{ false, false, false, false });
							D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
								device->GetResourceAllocationInfo(0, 1, &desc);

							size_t alignedStart = ((currentlyUsedMemory +
								(allocationInfo.Alignment - 1)) &
								~(allocationInfo.Alignment - 1));
							size_t alignedEnd = alignedStart +
								allocationInfo.SizeInBytes;

							if (alignedEnd > totalSize)
								break;

							currentlyUsedMemory = alignedEnd;

							auto index = component.CreateTexture(width, height,
								arraySize, mips);
							ASSERT_NE(index, ResourceIndex(-1));
							auto handle = component.GetTextureHandle(index);

							size_t textureByteSize = 0;
							unsigned int subresources =
								handle.resource->GetDesc().MipLevels * arraySize;

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
							}

							componentData.AddComponent(index, textureByteSize,
								handle.resource);
							unsigned int internalOffset = 0;

							for (unsigned int subresource = 0;
								subresource < subresources; ++subresource)
							{
								D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedFootprint;
								UINT nrOfRows = 0;
								UINT64 rowSize = 0;
								device->GetCopyableFootprints(&desc, subresource,
									1, 0, &placedFootprint, &nrOfRows,
									&rowSize, nullptr);

								unsigned char* currentData = data + currentDataOffset
									+ internalOffset;
								memset(currentData,
									static_cast<unsigned char>(subresource + 1),
									nrOfRows * rowSize);
								componentData.UpdateComponentData(index,
									currentData, textureInfo.second, subresource);
								internalOffset += nrOfRows * rowSize;
							}

							currentDataOffset += textureByteSize;
							++totalAdded;
						}
					}
				}
			}

			for (unsigned int currentFrame = 0; currentFrame < totalNrOfFrames;
				++currentFrame)
			{
				ID3D12Resource* readbackBuffer = CreateBuffer(device, totalSize, true);
				if (readbackBuffer == nullptr)
					throw "Cannot proceed with tests as readback buffer cannot be created";

				if (FAILED(commandStructure.allocator->Reset()))
					throw "Cannot proceed with tests as command allocator cannot be reset";

				if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
					throw "Cannot proceed with tests as command list cannot be reset";

				std::vector<D3D12_RESOURCE_BARRIER> barriers;
				componentData.PrepareUpdates(barriers, component);
				componentData.UpdateComponentResources(commandStructure.list,
					uploader, component, textureInfo.second, textureInfo.first);

				ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
				FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
				size_t totalChecked = 0;

				for (size_t i = 0; i < totalAdded; ++i)
				{
					if (FAILED(commandStructure.allocator->Reset()))
						throw "Cannot proceed with tests as command allocator cannot be reset";

					if (FAILED(commandStructure.list->Reset(commandStructure.allocator, nullptr)))
						throw "Cannot proceed with tests as command list cannot be reset";

					ResourceIndex index(i);
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
			}
		}

		fence->Release();
		delete[] data;
	};

	InitializationHelper(lambda);
}