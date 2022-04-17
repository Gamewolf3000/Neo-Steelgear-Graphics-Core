#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/ResourceUploader.h"

#include "D3D12Helper.h"

TEST(ResourceUploaderTest, DefaultInitialisable)
{
	ResourceUploader uploader;
}

bool InitialiseUploader(ResourceUploader& uploader, 
	ID3D12Device* device, size_t byteSize)
{
	uploader.Initialize(device, byteSize, AllocationStrategy::BEST_FIT);
	return true;
}

TEST(ResourceUploaderTest, RuntimeInitialisable)
{
	ResourceUploader uploader;
	ID3D12Device* device = nullptr;
	device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device cannot be created";

	if(!InitialiseUploader(uploader, device, 10000))
		FAIL() << "Cannot proceed with tests as a device cannot be created for the system";

	device->Release();
}

void FillBufferData(int arr[], int startIndex, int nrOfValues)
{
	for (int i = 0; i < nrOfValues; ++i)
		arr[i + startIndex] = i;
}

void CheckResourceData(ID3D12Resource* resource, UINT subresource,
	int data[], int startIndex, int stopIndex)
{
	int* mapped = nullptr;
	HRESULT hr = resource->Map(subresource, nullptr, 
		reinterpret_cast<void**>(&mapped));

	if (FAILED(hr))
		throw std::runtime_error("Mapping readback buffer failed!");

	for (int i = startIndex; i < stopIndex; ++i)
		ASSERT_EQ(mapped[i], data[i]);
}

void PrepareForNextBatch(SimpleCommandStructure& commandStructure,
	ID3D12Resource* targetResource)
{
	HRESULT hr = commandStructure.allocator->Reset();

	if (FAILED(hr))
		throw std::runtime_error("Could not reset command allocator");

	hr = commandStructure.list->Reset(commandStructure.allocator, nullptr);

	if (FAILED(hr))
		throw std::runtime_error("Could not reset command list");

	if (targetResource != nullptr)
	{
		TransitionResource(commandStructure.list, targetResource,
			D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	}
}

void ExecuteBufferCopy(SimpleCommandStructure& commandStructure,
	ID3D12Resource* targetBuffer, ID3D12Resource* readbackBuffer,
	int* data, int nrOfInts, UINT64& currentFenceValue,
	ID3D12Fence* fence)
{
	TransitionResource(commandStructure.list, targetBuffer,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commandStructure.list->CopyBufferRegion(readbackBuffer, 0,
		targetBuffer, 0, sizeof(int) * nrOfInts);
	ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
	FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
	CheckResourceData(readbackBuffer, 0, data, 0, nrOfInts);
}

TEST(ResourceUploaderTest, HandlesSimpleBufferUploads)
{
	ID3D12Device* device = nullptr;
	device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device cannot be created";

	ResourceUploader uploader;
	if (!InitialiseUploader(uploader, device, sizeof(int) * 1000))
		FAIL() << "Cannot proceed with tests as a device cannot be created for the system";

	ID3D12Resource* targetBuffer = CreateBuffer(device,
		sizeof(int) * 1000, false);
	ID3D12Resource* readbackBuffer = CreateBuffer(device,
		sizeof(int) * 1000, true);

	if (targetBuffer == nullptr || readbackBuffer == nullptr)
		FAIL() << "Cannot proceed with tests as resources could not be created";

	SimpleCommandStructure commandStructure;
	if(!CreateSimpleCommandStructure(commandStructure, device))
		FAIL() << "Cannot proceed with tests as command structure could not be created";

	ID3D12Fence* fence = nullptr;
	UINT64 currentFenceValue = 0;
	if (FAILED(device->CreateFence(currentFenceValue, 
		D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		FAIL() << "Cannot proceed with tests as fence could not be created";
	}

	const int NR_OF_INTS = 1000;
	int data[NR_OF_INTS];
	FillBufferData(data, 0, NR_OF_INTS);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer,
		commandStructure.list, data, 0, sizeof(int) * NR_OF_INTS,
		alignof(int)), true);

	ExecuteBufferCopy(commandStructure, targetBuffer, readbackBuffer,
		data, NR_OF_INTS, currentFenceValue, fence);
	PrepareForNextBatch(commandStructure, targetBuffer);
	uploader.RestoreUsedMemory();

	for (int i = 0; i < 10; ++i)
	{
		FillBufferData(data, i * 100, 100);
		ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer, 
			commandStructure.list, data, i * 100 * sizeof(int),
			sizeof(int) * 100, alignof(int)), true);
	}

	ExecuteBufferCopy(commandStructure, targetBuffer, readbackBuffer,
		data, NR_OF_INTS, currentFenceValue, fence);

	device->Release();
	targetBuffer->Release();
	readbackBuffer->Release();
	fence->Release();
}

TEST(ResourceUploaderTest, HandlesAdvancedBufferUploads)
{
	ID3D12Device* device = nullptr;
	device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device cannot be created";

	ResourceUploader uploader;
	if (!InitialiseUploader(uploader, device, 1024))
		FAIL() << "Cannot proceed with tests as a device cannot be created for the system";

	ID3D12Resource* targetBuffer = CreateBuffer(device,
		1024, false);
	ID3D12Resource* readbackBuffer = CreateBuffer(device,
		1024, true);

	if (targetBuffer == nullptr || readbackBuffer == nullptr)
		FAIL() << "Cannot proceed with tests as resources could not be created";

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device))
		FAIL() << "Cannot proceed with tests as command structure could not be created";

	ID3D12Fence* fence = nullptr;
	UINT64 currentFenceValue = 0;
	if (FAILED(device->CreateFence(currentFenceValue,
		D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		FAIL() << "Cannot proceed with tests as fence could not be created";
	}

	const int NR_OF_INTS = 256;
	int data[NR_OF_INTS];
	FillBufferData(data, 0, NR_OF_INTS);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer, 
		commandStructure.list, data, 0, sizeof(int) * NR_OF_INTS,
		alignof(int)), true);

	ExecuteBufferCopy(commandStructure, targetBuffer, readbackBuffer,
		data, NR_OF_INTS, currentFenceValue, fence);
	PrepareForNextBatch(commandStructure, targetBuffer);
	uploader.RestoreUsedMemory();
	//Above is just to guarantee the "default" data in the buffer

	FillBufferData(data, 0, 10);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer,
		commandStructure.list, data, 0, sizeof(int) * 10,
		alignof(int)), true);

	FillBufferData(data, 64, 64);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer,
		commandStructure.list, data, sizeof(int) * 64, sizeof(int) * 64,
		256), true);

	FillBufferData(data, 128, 64);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer,
		commandStructure.list, data, sizeof(int) * 128, sizeof(int) * 64,
		256), true);

	FillBufferData(data, 192, 64);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer, 
		commandStructure.list, data, sizeof(int) * 192, sizeof(int) * 64,
		256), true);

	FillBufferData(data, 16, 16);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer,
		commandStructure.list, data, sizeof(int) * 16, sizeof(int) * 16,
		64), true);

	FillBufferData(data, 32, 8);
	ASSERT_EQ(uploader.UploadBufferResourceData(targetBuffer, 
		commandStructure.list, data, sizeof(int) * 32, sizeof(int) * 8,
		32), true);

	ExecuteBufferCopy(commandStructure, targetBuffer, readbackBuffer,
		data, NR_OF_INTS, currentFenceValue, fence);

	device->Release();
	targetBuffer->Release();
	readbackBuffer->Release();
	fence->Release();
}

unsigned int FillTexture2DData(unsigned char* data, unsigned int width,
	unsigned int height, unsigned int bytesPerTexel, unsigned int mipLevels,
	unsigned int offsetInData)
{
	unsigned int currentOffset = offsetInData;
	unsigned int currentWidth = width;
	unsigned int currentHeight = height;
	unsigned char currentValue = 0;
	for (unsigned int i = 0; i < mipLevels; ++i)
	{
		for (unsigned int h = 0; h < currentHeight; ++h)
		{
			for (unsigned int w = 0; w < currentWidth; ++w)
			{
				for (int c = 0; c < bytesPerTexel; ++c)
				{
					data[currentOffset + c] = currentValue++;
				}

				currentOffset += bytesPerTexel;
			}
		}

		currentWidth = floor(currentWidth / 2.0f);
		currentHeight = floor(currentHeight / 2.0f);
	}

	return currentOffset;
}

void CheckResourceData(ID3D12Resource* readbackBuffer, 
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints,
	std::vector<UINT> rows, std::vector<UINT64> rowSizes, unsigned char* data)
{
	unsigned char* mapped = nullptr;
	HRESULT hr = readbackBuffer->Map(0, nullptr,
		reinterpret_cast<void**>(&mapped));

	unsigned char* currentData = data;

	for (unsigned int subresource = 0; subresource < footprints.size(); ++subresource)
	{
		unsigned char* currentMapped = mapped + footprints[subresource].Offset;
		for (unsigned int row = 0; row < rows[subresource]; ++row)
		{
			ASSERT_EQ(memcmp(currentMapped, currentData, rowSizes[subresource]), 0);
			currentMapped += footprints[subresource].Footprint.RowPitch;
			currentData += rowSizes[subresource];
		}
	}
}

void ExecuteTexture2DCopy(ID3D12Device* device, 
	SimpleCommandStructure& commandStructure, ID3D12Resource* targetTexture,
	unsigned char* data, unsigned int mipLevels, UINT64& currentFenceValue,
	ID3D12Fence* fence)
{
	TransitionResource(commandStructure.list, targetTexture,
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);

	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mipLevels);
	std::vector<UINT> rows(mipLevels);
	std::vector<UINT64> rowSizes(mipLevels);
	std::vector<UINT64> totalSizes(mipLevels);
	device->GetCopyableFootprints(&targetTexture->GetDesc(), 0, mipLevels, 0,
		footprints.data(), rows.data(), rowSizes.data(), totalSizes.data());

	UINT64 readbackBufferSize = 0;
	for (auto& mipSize : totalSizes)
		readbackBufferSize += mipSize;

	ID3D12Resource* readbackBuffer = CreateBuffer(device, readbackBufferSize, true);

	if (readbackBuffer == nullptr)
		FAIL() << "Cannot proceed with tests as resources could not be created";

	for (unsigned int mipLevel = 0; mipLevel < mipLevels; ++mipLevel)
	{
		D3D12_TEXTURE_COPY_LOCATION destination;
		destination.pResource = readbackBuffer;
		destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		destination.PlacedFootprint = footprints[mipLevel];

		D3D12_TEXTURE_COPY_LOCATION source;
		source.pResource = targetTexture;
		source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		source.SubresourceIndex = mipLevel;

		commandStructure.list->CopyTextureRegion(&destination, 0, 0, 0,
			&source, nullptr);
	}

	ExecuteGraphicsCommandList(commandStructure.list, commandStructure.queue);
	FlushCommandQueue(currentFenceValue, commandStructure.queue, fence);
	CheckResourceData(readbackBuffer, footprints, rows, rowSizes, data);
	readbackBuffer->Release();
}

TEST(ResourceUploaderTest, HandlesSimpleTextureUploads)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = 4096;
	resourceDesc.Height = 4096;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Device* device = nullptr;
	device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device cannot be created";

	std::vector<std::pair<short, DXGI_FORMAT>> formats = { 
		{4, DXGI_FORMAT_R8G8B8A8_UNORM}, {4, DXGI_FORMAT_R16G16_UINT},
		{4, DXGI_FORMAT_R32_UINT}, {16, DXGI_FORMAT_R32G32B32A32_UINT}, 
		{8, DXGI_FORMAT_R32G32_UINT}, {12, DXGI_FORMAT_R32G32B32_UINT} };
	std::vector<std::pair<UINT, UINT>> dimensions = { {1, 1}, {2, 2}, {4, 4}, 
		{8, 8}, {16, 16}, {32, 32}, {64, 64}, {128, 128}, {256, 256}, {512, 512},
		{1024, 1024}, {2048, 2048}, {4096, 4096}, {1, 2}, {1, 4}, {1, 8}, {1, 16},
		{2, 1}, {4, 1}, {8, 1}, {16, 1}, {16, 2}, {2, 16}, {4, 8}, {8, 4}, 
		{1024, 256}, {512, 2048}, {4096, 1}, {2, 2048}, {256, 512}, {128, 4096},
		{100, 100}, {333, 333}, {13, 37}, {24, 7}, {123, 456}, {987, 654} };

	D3D12_RESOURCE_ALLOCATION_INFO allocationInfoLargest =
		device->GetResourceAllocationInfo(0, 1, &resourceDesc);
	ResourceUploader uploader;
	if (!InitialiseUploader(uploader, device, allocationInfoLargest.SizeInBytes * 2)) // mul 2 for mips
		FAIL() << "Cannot proceed with tests as a device cannot be created for the system";

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device))
		FAIL() << "Cannot proceed with tests as command structure could not be created";

	ID3D12Fence* fence = nullptr;
	UINT64 currentFenceValue = 0;
	if (FAILED(device->CreateFence(currentFenceValue,
		D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		FAIL() << "Cannot proceed with tests as fence could not be created";
	}

	for (auto& format : formats)
	{
		for (auto& dimension : dimensions)
		{
			D3D12_RESOURCE_DESC description = resourceDesc;
			description.Width = dimension.first;
			description.Height = dimension.second;
			description.Format = format.second;

			unsigned int nrOfMips =
				max(1, floor(log2(min(description.Width, description.Height))));
			description.MipLevels = nrOfMips;

			ID3D12Resource* targetTexture;
			targetTexture = CreateTexture2D(device, description);
			if (targetTexture == nullptr)
				FAIL() << "Cannot proceed with tests as resources could not be created";

			unsigned char* data = new unsigned char[description.Width *
				description.Height * format.first * 2]; // multiplied by 2 to always have for mips
			FillTexture2DData(data, description.Width, description.Height,
				format.first, nrOfMips, 0);

			TextureUploadInfo uploadInfo;
			uploadInfo.width = description.Width;
			uploadInfo.height = description.Height;
			uploadInfo.texelSizeInBytes = format.first;
			uploadInfo.format = format.second;
			unsigned char* currentDataHead = data;

			for (unsigned int miplevel = 0; miplevel < nrOfMips; ++miplevel)
			{
				if (!uploader.UploadTextureResourceData(targetTexture,
					commandStructure.list, currentDataHead, uploadInfo, miplevel))
				{
					uploader.RestoreUsedMemory();
					ASSERT_TRUE(uploader.UploadTextureResourceData(targetTexture,
						commandStructure.list, currentDataHead, uploadInfo, miplevel));
				}

				currentDataHead += uploadInfo.width * uploadInfo.height *
					uploadInfo.texelSizeInBytes;
				uploadInfo.width = floor(uploadInfo.width / 2.0f);
				uploadInfo.height = floor(uploadInfo.height / 2.0f);
			}

			ExecuteTexture2DCopy(device, commandStructure, targetTexture,
				data, nrOfMips, currentFenceValue, fence);
			PrepareForNextBatch(commandStructure, nullptr);
			targetTexture->Release();
			delete[] data;
		}
	}

	device->Release();
	fence->Release();
}

TEST(ResourceUploaderTest, HandlesAdvancedTextureUploads)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = 4096;
	resourceDesc.Height = 4096;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Device* device = nullptr;
	device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device cannot be created";

	std::vector<std::pair<short, DXGI_FORMAT>> formats = {
		{4, DXGI_FORMAT_R8G8B8A8_UNORM}, {4, DXGI_FORMAT_R16G16_UINT},
		{4, DXGI_FORMAT_R32_UINT}, {16, DXGI_FORMAT_R32G32B32A32_UINT},
		{8, DXGI_FORMAT_R32G32_UINT}, {12, DXGI_FORMAT_R32G32B32_UINT} };
	std::vector<std::pair<UINT, UINT>> dimensions = { {2, 2}, {4, 4},
		{8, 8}, {16, 16}, {32, 32}, {64, 64}, {128, 128}, {256, 256}, {512, 512},
		{1024, 1024}, {2048, 2048}, {4096, 4096}, {1, 2}, {1, 4}, {1, 8}, {1, 16},
		{4, 2}, {8, 2}, {16, 2}, {2, 16}, {4, 8}, {8, 4},
		{1024, 256}, {512, 2048}, {4096, 1}, {2, 2048}, {256, 512}, {128, 4096},
		{100, 100}, {333, 333}, {13, 37}, {24, 7}, {123, 456}, {987, 654} };

	D3D12_RESOURCE_ALLOCATION_INFO allocationInfoLargest =
		device->GetResourceAllocationInfo(0, 1, &resourceDesc);
	ResourceUploader uploader;
	if (!InitialiseUploader(uploader, device, allocationInfoLargest.SizeInBytes))
		FAIL() << "Cannot proceed with tests as a device cannot be created for the system";

	SimpleCommandStructure commandStructure;
	if (!CreateSimpleCommandStructure(commandStructure, device))
		FAIL() << "Cannot proceed with tests as command structure could not be created";

	ID3D12Fence* fence = nullptr;
	UINT64 currentFenceValue = 0;
	if (FAILED(device->CreateFence(currentFenceValue,
		D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		FAIL() << "Cannot proceed with tests as fence could not be created";
	}

	for (auto& format : formats)
	{
		for (auto& dimension : dimensions)
		{
			D3D12_RESOURCE_DESC description = resourceDesc;
			description.Width = dimension.first;
			description.Height = dimension.second;
			description.Format = format.second;

			ID3D12Resource* targetTexture = nullptr;
			targetTexture = CreateTexture2D(device, description);
			if (targetTexture == nullptr)
				FAIL() << "Cannot proceed with tests as resources could not be created";
			
			unsigned int dataSize = description.Width * description.Height *
				format.first;
			unsigned char* data = new unsigned char[dataSize];
			FillTexture2DData(data, description.Width, description.Height,
				format.first, 1, 0);

			for (unsigned int nrOfDivisions = 2; 
				nrOfDivisions <= 10 && nrOfDivisions <= description.Height;
				++nrOfDivisions)
			{
				unsigned char* currentDataHead = data;
				unsigned int rowsLeft = description.Height;
				unsigned int rowsPerDivision = description.Height / nrOfDivisions;
				for (unsigned int division = 0; division < nrOfDivisions - 1; 
					++division)
				{
					TextureUploadInfo uploadInfo;
					uploadInfo.width = description.Width;
					uploadInfo.height = rowsPerDivision;
					uploadInfo.texelSizeInBytes = format.first;
					uploadInfo.format = format.second;
					uploadInfo.offsetHeight = division * rowsPerDivision;

					ASSERT_TRUE(uploader.UploadTextureResourceData(targetTexture,
						commandStructure.list, currentDataHead, uploadInfo, 0));

					currentDataHead += rowsPerDivision * description.Width * format.first;
					rowsLeft -= rowsPerDivision;
				}

				TextureUploadInfo uploadInfo;
				uploadInfo.width = description.Width;
				uploadInfo.height = rowsLeft;
				uploadInfo.texelSizeInBytes = format.first;
				uploadInfo.format = format.second;
				uploadInfo.offsetHeight = (nrOfDivisions - 1) * rowsPerDivision;

				ASSERT_TRUE(uploader.UploadTextureResourceData(targetTexture,
					commandStructure.list, currentDataHead, uploadInfo, 0));

				ExecuteTexture2DCopy(device, commandStructure, targetTexture,
					data, 1, currentFenceValue, fence);

				if(nrOfDivisions + 1 <= 10 && nrOfDivisions + 1 <= description.Height)
					PrepareForNextBatch(commandStructure, targetTexture);
				else
					PrepareForNextBatch(commandStructure, nullptr);

				uploader.RestoreUsedMemory();
			}

			targetTexture->Release();
			delete[] data;
		}
	}

	device->Release();
	fence->Release();
}