#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/Texture2DComponent.h"
#include "../Neo Steelgear Graphics Core/MultiHeapAllocatorGPU.h"

#include "D3D12Helper.h"

TEST(Texture2DComponentTest, DefaultInitialisable)
{
	Texture2DComponent textureComponent;
}

void InitializationHelper(std::function<void(ID3D12Device*, 
	const TextureComponentInfo&,
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	std::array<size_t, 4> heapSizes = { 1000000, 10000000, 100000000, 1000000000 };

	std::vector<std::pair<DXGI_FORMAT, std::uint8_t>> textureInfos = { 
		{DXGI_FORMAT_R8G8B8A8_UNORM, 4}, {DXGI_FORMAT_D32_FLOAT, 4},
		{DXGI_FORMAT_R32G32B32A32_FLOAT, 16}, {DXGI_FORMAT_R32G32B32_UINT, 12} };

	std::vector<AllowedViews> ViewCombinations = {
		{true, false, false, false}, {false, true, false, false},
		{true, true, false, false}, {false, false, true, false},
		{true, false, true, false}, {false, true, true, false},
		{true, true, true, false}, {false, false, false, true} };

	for (auto& heapSize : heapSizes)
	{
		for (auto& textureInfo : textureInfos)
		{
			for (auto& allowedViews : ViewCombinations)
			{
				bool unacceptableCombination = false;
				unacceptableCombination |=
					textureInfo.first != DXGI_FORMAT_D32_FLOAT &&
					allowedViews.dsv;
				unacceptableCombination |=
					textureInfo.first == DXGI_FORMAT_D32_FLOAT &&
					!allowedViews.dsv;
				unacceptableCombination |=
					textureInfo.first == DXGI_FORMAT_R32G32B32_UINT &&
					(allowedViews.rtv || allowedViews.uav);

				if (unacceptableCombination)
					continue;

				size_t maxNrOfTextures = heapSize / 10000;
				std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>
					descriptorAllocationInfo = 
					CreateDescriptorAllocationInfo(maxNrOfTextures, allowedViews);

				ResourceComponentMemoryInfo memoryInfo;
				memoryInfo.initialMinimumHeapSize = heapSize;
				memoryInfo.expansionMinimumSize = 0;
				memoryInfo.heapAllocator = &heapAllocator;
				TextureComponentInfo componentInfo(textureInfo.first,
					textureInfo.second, memoryInfo);

				func(device, componentInfo, descriptorAllocationInfo);
			}
		}
	}

	device->Release();
}

TEST(Texture2DComponentTest, RuntimeInitialisable)
{
	auto lambda = [](ID3D12Device* device, const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
		descriptorAllocationInfo)
	{
		Texture2DComponent textureComponent;
		textureComponent.Initialize(device, textureInfo, descriptorAllocationInfo);
	};

	InitializationHelper(lambda);
}

TEST(Texture2DComponentTest, CreatesTexturesCorrectly)
{
	auto lambda = [](ID3D12Device* device, const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
		descriptorAllocationInfo)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };

		for (auto width : dimensions)
		{
			size_t currentlyUsedMemory = 0;
			size_t currentExpectedIndex = 0;
			Texture2DComponent textureComponent;
			textureComponent.Initialize(device, textureInfo,
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
							mips, arraySize, textureInfo.format, allowedViews);
						D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
							device->GetResourceAllocationInfo(0, 1, &desc);

						size_t alignedStart = ((currentlyUsedMemory +
							(allocationInfo.Alignment - 1)) &
							~(allocationInfo.Alignment - 1));
						size_t alignedEnd = alignedStart +
							allocationInfo.SizeInBytes;

						if (alignedEnd > textureInfo.memoryInfo.initialMinimumHeapSize)
							break;

						currentlyUsedMemory = alignedEnd;
						ResourceIndex index = textureComponent.CreateTexture(width,
							height, arraySize, mips);
						ASSERT_EQ(index.allocatorIdentifier.heapChunkIndex, 0);
						ASSERT_EQ(index.allocatorIdentifier.internalIndex,
							currentExpectedIndex);
						ASSERT_EQ(index.descriptorIndex, currentExpectedIndex);
						++currentExpectedIndex;
					}
				}
			}
		}
	};

	InitializationHelper(lambda);
}

struct TextureAllocationHelpStruct
{
	UINT width;
	UINT height;
	UINT16 arraySize;
	UINT16 mips;
};

TEST(Texture2DComponentTest, RemovesTexturesCorrectly)
{
	auto lambda = [](ID3D12Device* device, const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
		descriptorAllocationInfo)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };

		for (auto width : dimensions)
		{
			size_t currentlyUsedMemory = 0;
			size_t currentExpectedIndex = 0;
			std::vector<std::pair<ResourceIndex, TextureAllocationHelpStruct>> allocations;
			Texture2DComponent textureComponent;
			textureComponent.Initialize(device, textureInfo,
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
							mips, arraySize, textureInfo.format, allowedViews);
						D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
							device->GetResourceAllocationInfo(0, 1, &desc);

						size_t alignedStart = ((currentlyUsedMemory +
							(allocationInfo.Alignment - 1)) &
							~(allocationInfo.Alignment - 1));
						size_t alignedEnd = alignedStart +
							allocationInfo.SizeInBytes;

						if (alignedEnd > textureInfo.memoryInfo.initialMinimumHeapSize)
							break;

						currentlyUsedMemory = alignedEnd;
						ResourceIndex index = textureComponent.CreateTexture(width,
							height, arraySize, mips);
						ASSERT_EQ(index.allocatorIdentifier.heapChunkIndex, 0);
						ASSERT_NE(index.allocatorIdentifier.internalIndex, size_t(-1));
						ASSERT_NE(index.descriptorIndex, size_t(-1));
						TextureAllocationHelpStruct allocationHelper = { width, height,
							arraySize, mips };
						allocations.push_back(std::make_pair(index, allocationHelper));
					}
				}
			}

			for (auto& allocation : allocations)
				textureComponent.RemoveComponent(allocation.first);

			for (auto& allocation : allocations)
			{
				TextureAllocationHelpStruct allocationHelper = allocation.second;
				ResourceIndex index = textureComponent.CreateTexture(allocationHelper.width,
					allocationHelper.height, allocationHelper.arraySize, allocationHelper.mips);
				ASSERT_EQ(index.allocatorIdentifier.heapChunkIndex, 0);
				ASSERT_LE(index.allocatorIdentifier.internalIndex,
					allocations.back().first.allocatorIdentifier.internalIndex + 1);
				ASSERT_LE(index.descriptorIndex, 
					allocations.back().first.descriptorIndex + 1);
			}
		}
	};

	InitializationHelper(lambda);
}

TEST(Texture2DComponentTest, ExpandsCorrectly)
{
	auto lambda = [](ID3D12Device* device, const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
		descriptorAllocationInfo)
	{
		std::array<UINT, 2> dimensions = { 1024, 2048 };
		std::array<UINT16, 2> arraySizes = { 2, 3 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };

		for (auto width : dimensions)
		{
			size_t largestChunkIndex = 0;
			Texture2DComponent textureComponent;
			TextureComponentInfo expandable = textureInfo;
			expandable.memoryInfo.initialMinimumHeapSize /= 10;
			expandable.memoryInfo.expansionMinimumSize = 
				expandable.memoryInfo.initialMinimumHeapSize;
			textureComponent.Initialize(device, expandable,
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
							mips, arraySize, textureInfo.format, allowedViews);

						ResourceIndex index = textureComponent.CreateTexture(width,
							height, arraySize, mips);
						largestChunkIndex = std::max<size_t>(largestChunkIndex,
							index.allocatorIdentifier.heapChunkIndex);
					}
				}
			}

			ASSERT_GE(largestChunkIndex, 1);
		}
	};

	InitializationHelper(lambda);
}