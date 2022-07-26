#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/TextureAllocator.h"
#include "../Neo Steelgear Graphics Core/MultiHeapAllocatorGPU.h"

#include "D3D12Helper.h"

TEST(TextureAllocatorTest, DefaultInitialisable)
{
	TextureAllocator textureAllocator;
}

void InitializationHelper(std::function<void(ID3D12Device*, size_t,
	const std::pair<DXGI_FORMAT, std::uint8_t>&, const AllowedViews&,
	HeapAllocatorGPU*)> func)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	MultiHeapAllocatorGPU heapAllocator;
	heapAllocator.Initialize(device);

	std::array<size_t, 4> heapSizes = { 1000000, 10000000, 100000000, 1000000000 };

	std::vector<std::pair<DXGI_FORMAT, std::uint8_t>> textureInfos = { 
		{DXGI_FORMAT_R8G8B8A8_UNORM, 4}, {DXGI_FORMAT_D32_FLOAT, 4},
		{DXGI_FORMAT_R32G32B32A32_FLOAT, 16}, {DXGI_FORMAT_R32G32_TYPELESS, 8},
		{DXGI_FORMAT_R32G32B32_UINT, 12} };

	std::vector<AllowedViews> ViewCombinations = { {false, false, false, false},
		{true, false, false, false}, {false, true, false, false},
		{true, true, false, false}, {false, false, true, false}, 
		{true, false, true, false}, {false, true, true, false}, 
		{true, true, true, false}, {false, false, false, true}, 
		{true, false, false, true} };

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

				func(device, heapSize, textureInfo, allowedViews, &heapAllocator);
			}
		}
	}

	device->Release();
}

TEST(TextureAllocatorTest, RuntimeInitialisable)
{
	auto lambda = [](ID3D12Device* device, size_t heapSize,
		const std::pair<DXGI_FORMAT, std::uint8_t>& textureInfo,
		const AllowedViews& allowedViews, HeapAllocatorGPU* heapAllocator)
	{
		(void)textureInfo;
		TextureAllocator textureAllocator;
		textureAllocator.Initialize(device, allowedViews, heapSize, 0, 
			heapAllocator);
	};

	InitializationHelper(lambda);
}

TEST(TextureAllocatorTest, CorrectlyAllocatesTextures)
{
	auto lambda = [](ID3D12Device* device, size_t heapSize,
		const std::pair<DXGI_FORMAT, std::uint8_t>& textureInfo,
		const AllowedViews& allowedViews, HeapAllocatorGPU* heapAllocator)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };

		for (auto width : dimensions)
		{
			size_t currentlyUsedMemory = 0;
			size_t currentExpectedIndex = 0;
			TextureAllocator textureAllocator;
			textureAllocator.Initialize(device, allowedViews, heapSize,
				0, heapAllocator);

			for (auto height : dimensions)
			{
				for (auto mips : mipLevels)
				{
					for (auto arraySize : arraySizes)
					{
						D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width, height,
							mips, arraySize, textureInfo.first, allowedViews);
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
						TextureAllocationInfo textureAllocationInfo(textureInfo.first, 
							textureInfo.second, width, height, arraySize, mips);
						ResourceIdentifier identifier = 
							textureAllocator.AllocateTexture(
							textureAllocationInfo);

						ASSERT_EQ(identifier.heapChunkIndex, 0);
						ASSERT_EQ(identifier.internalIndex, currentExpectedIndex++);
					}
				}
			}
		}

	};

	InitializationHelper(lambda);
}

TEST(TextureAllocatorTest, CorrectlyDeallocatesTextures)
{
	auto lambda = [](ID3D12Device* device, size_t heapSize,
		const std::pair<DXGI_FORMAT, std::uint8_t>& textureInfo,
		const AllowedViews& allowedViews, HeapAllocatorGPU* heapAllocator)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };

		for (auto width : dimensions)
		{
			size_t currentlyUsedMemory = 0;
			std::vector<std::pair<ResourceIdentifier,
				TextureAllocationInfo>> allocations;
			TextureAllocator textureAllocator;
			textureAllocator.Initialize(device, allowedViews, heapSize, 0,
				heapAllocator);

			for (auto height : dimensions)
			{
				for (auto mips : mipLevels)
				{
					for (auto arraySize : arraySizes)
					{
						D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width, height,
							mips, arraySize, textureInfo.first, allowedViews);
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
						TextureAllocationInfo textureAllocationInfo(textureInfo.first,
							textureInfo.second, width, height, arraySize, mips);
						ResourceIdentifier identifier = 
							textureAllocator.AllocateTexture(
							textureAllocationInfo);
						ASSERT_EQ(identifier.heapChunkIndex, 0);
						ASSERT_NE(identifier.internalIndex, size_t(-1));
						allocations.push_back(
							std::make_pair(identifier, textureAllocationInfo));
					}
				}
			}

			for (auto& allocation : allocations)
				textureAllocator.DeallocateTexture(allocation.first);

			for (auto& allocation : allocations)
			{
				ResourceIdentifier identifier = 
					textureAllocator.AllocateTexture(allocation.second);

				ASSERT_EQ(identifier.heapChunkIndex, 0);
				ASSERT_LE(identifier.internalIndex, 
					allocations.back().first.internalIndex + 1);
			}
		}

	};

	InitializationHelper(lambda);
}

TEST(TextureAllocatorTest, CorrectlyExpands)
{
	auto lambda = [](ID3D12Device* device, size_t heapSize,
		const std::pair<DXGI_FORMAT, std::uint8_t>& textureInfo,
		const AllowedViews& allowedViews, HeapAllocatorGPU* heapAllocator)
	{
		std::array<UINT, 7> dimensions = { 1, 2, 5, 20, 128, 499, 1024 };
		std::array<UINT16, 5> arraySizes = { 1, 2, 7, 8, 10 };
		std::array<UINT16, 2> mipLevels = { 1, 0 };

		for (auto width : dimensions)
		{
			std::vector<size_t> expectedIndices = { 0 };
			TextureAllocator textureAllocator;
			textureAllocator.Initialize(device, allowedViews, 
				heapSize / 4, heapSize / 4, heapAllocator);

			for (auto height : dimensions)
			{
				for (auto mips : mipLevels)
				{
					for (auto arraySize : arraySizes)
					{
						D3D12_RESOURCE_DESC desc = CreateTexture2DDesc(width, height,
							mips, arraySize, textureInfo.first, allowedViews);
						D3D12_RESOURCE_ALLOCATION_INFO allocationInfo =
							device->GetResourceAllocationInfo(0, 1, &desc);

						TextureAllocationInfo textureAllocationInfo(textureInfo.first,
							textureInfo.second, width, height, arraySize, mips);
						ResourceIdentifier identifier = textureAllocator.AllocateTexture(
							textureAllocationInfo);

						if (identifier.heapChunkIndex == expectedIndices.size())
							expectedIndices.push_back(0);

						ASSERT_EQ(identifier.internalIndex, 
							expectedIndices[identifier.heapChunkIndex]++);
					}
				}
			}
		}
	};

	InitializationHelper(lambda);
}