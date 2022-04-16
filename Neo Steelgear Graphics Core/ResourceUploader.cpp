#include "ResourceUploader.h"

#include <stdexcept>

void ResourceUploader::AllocateBuffer(ID3D12Heap* heap, size_t heapOffset)
{
	D3D12_RESOURCE_DESC desc;

	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = totalMemory;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = device->CreatePlacedResource(heap, heapOffset, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer));

	if (FAILED(hr))
		throw std::runtime_error("Could not create placed upload resource");
}

void ResourceUploader::AllocateBuffer()
{
	D3D12_HEAP_PROPERTIES heapProperties;
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProperties.CreationNodeMask = 0;
	heapProperties.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = totalMemory;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;
	
	HRESULT hr = device->CreateCommittedResource(&heapProperties, 
		D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(&buffer));

	if (FAILED(hr))
		throw std::runtime_error("Could not create committed upload resource");
}

size_t ResourceUploader::AlignAdress(size_t adress, size_t alignment)
{
	if ((0 == alignment) || (alignment & (alignment - 1)))
		throw std::runtime_error("Error: non-pow2 alignment");

	return ((adress + (alignment - 1)) & ~(alignment - 1));
}

void ResourceUploader::CopyBufferRegionToResource(ID3D12Resource* toUploadTo,
	ID3D12GraphicsCommandList* commandList, void* data, size_t offsetFromStart,
	size_t dataSize, size_t alignment, size_t freeChunkIndex)
{
	size_t alignedOffset = AlignAdress(
		uploadChunks.GetStartOfChunk(freeChunkIndex), alignment);
	memcpy(mappedPtr + alignedOffset, data, dataSize);
	commandList->CopyBufferRegion(toUploadTo, offsetFromStart, buffer,
		alignedOffset, dataSize);
}

void ResourceUploader::MemcpyTextureData(unsigned char* destinationStart,
	unsigned char* sourceStart, const TextureUploadInfo& uploadInfo)
{
	size_t rowPitch = AlignAdress(uploadInfo.width * uploadInfo.texelSizeInBytes,
		D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	size_t sliceSize = static_cast<size_t>(uploadInfo.width) * uploadInfo.height *
		uploadInfo.texelSizeInBytes;
	size_t rowSize = uploadInfo.width * uploadInfo.texelSizeInBytes;

	for (size_t z = 0; z < uploadInfo.depth; ++z)
	{
		for (size_t y = 0; y < uploadInfo.height; ++y)
		{
			unsigned char* currentDestination = destinationStart;
			currentDestination += z * rowPitch * uploadInfo.height;
			currentDestination += y * rowPitch;

			unsigned char* currentSource = sourceStart;
			currentSource += z * sliceSize + y * rowSize;

			memcpy(currentDestination, currentSource,
				uploadInfo.width * uploadInfo.texelSizeInBytes);
		}
	}
}

void ResourceUploader::CopyTextureRegionToResource(ID3D12Resource* toUploadTo,
	ID3D12GraphicsCommandList* commandList, void* data,
	const TextureUploadInfo& uploadInfo, unsigned int subresourceIndex,
	size_t freeChunkIndex)
{
	size_t alignedOffset = AlignAdress(uploadChunks.GetStartOfChunk(freeChunkIndex),
		D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
	size_t rowPitch = AlignAdress(uploadInfo.width * uploadInfo.texelSizeInBytes,
		D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	D3D12_TEXTURE_COPY_LOCATION destination;
	destination.pResource = toUploadTo;
	destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destination.SubresourceIndex = subresourceIndex;

	D3D12_TEXTURE_COPY_LOCATION source;
	source.pResource = buffer;
	source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	source.PlacedFootprint.Offset = alignedOffset;
	source.PlacedFootprint.Footprint.Width = uploadInfo.width;
	source.PlacedFootprint.Footprint.Height = uploadInfo.height;
	source.PlacedFootprint.Footprint.Depth = uploadInfo.depth;
	source.PlacedFootprint.Footprint.RowPitch = static_cast<unsigned int>(rowPitch);
	source.PlacedFootprint.Footprint.Format = uploadInfo.format;

	MemcpyTextureData(mappedPtr + alignedOffset, static_cast<unsigned char*>(data),
		uploadInfo);

	commandList->CopyTextureRegion(&destination, uploadInfo.offsetWidth,
		uploadInfo.offsetHeight, uploadInfo.offsetDepth, &source, nullptr);
}

ResourceUploader::ResourceUploader(ResourceUploader&& other) noexcept : 
	device(other.device), buffer(std::move(other.buffer)), 
	mappedPtr(other.mappedPtr), latestUploadId(other.latestUploadId),
	totalMemory(other.totalMemory), allocationStrategy(other.allocationStrategy), 
	uploadChunks(std::move(other.uploadChunks))
{
	other.device = nullptr;
	other.mappedPtr = nullptr;
	other.latestUploadId = 0;
	other.totalMemory = 0;
}

ResourceUploader& ResourceUploader::operator=(ResourceUploader&& other) noexcept
{
	if (this != &other)
	{
		device = other.device;
		buffer = std::move(other.buffer);
		mappedPtr = other.mappedPtr;
		latestUploadId = other.latestUploadId;
		totalMemory = other.totalMemory;
		allocationStrategy = other.allocationStrategy;
		uploadChunks = std::move(other.uploadChunks);

		other.device = nullptr;
		other.mappedPtr = nullptr;
		other.latestUploadId = 0;
		other.totalMemory = 0;
	}

	return *this;
}

void ResourceUploader::Initialize(ID3D12Device* deviceToUse, ID3D12Heap* heap,
	size_t startOffset, size_t endOffset, AllocationStrategy strategy)
{
	device = deviceToUse;
	totalMemory = endOffset - startOffset;
	allocationStrategy = strategy;
	uploadChunks.Initialize(totalMemory);
	AllocateBuffer(heap, startOffset);
	D3D12_RANGE nothing = { 0, 0 }; // We only write, we do not read
	buffer->Map(0, &nothing, reinterpret_cast<void**>(&mappedPtr));
}

void ResourceUploader::Initialize(ID3D12Device* deviceToUse, size_t heapSize,
	AllocationStrategy strategy)
{
	device = deviceToUse;
	totalMemory = heapSize;
	allocationStrategy = strategy;
	uploadChunks.Initialize(totalMemory);
	AllocateBuffer();
	D3D12_RANGE nothing = { 0, 0 }; // We only write, we do not read
	buffer->Map(0, &nothing, reinterpret_cast<void**>(&mappedPtr));
}

bool ResourceUploader::UploadBufferResourceData(ID3D12Resource* toUploadTo,
	ID3D12GraphicsCommandList* commandList, void* data, size_t offsetFromStart,
	size_t dataSize, size_t alignment)
{
	size_t chunkIndex = uploadChunks.AllocateChunk(dataSize, allocationStrategy,
		alignment);

	if (chunkIndex == size_t(-1))
		return false;

	CopyBufferRegionToResource(toUploadTo, commandList, data, offsetFromStart,
		dataSize, alignment, chunkIndex);

	return true;
}

bool ResourceUploader::UploadTextureResourceData(ID3D12Resource* toUploadTo,
	ID3D12GraphicsCommandList* commandList, void* data,
	const TextureUploadInfo& uploadInfo, unsigned int subresourceIndex)
{
	size_t totalSize = AlignAdress(uploadInfo.width * uploadInfo.texelSizeInBytes,
		D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	totalSize *= static_cast<size_t>(uploadInfo.height) * uploadInfo.depth;
	size_t chunkIndex = uploadChunks.AllocateChunk(totalSize, allocationStrategy,
		D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

	if (chunkIndex == size_t(-1))
		return false;

	CopyTextureRegionToResource(toUploadTo, commandList, data, uploadInfo,
		subresourceIndex, chunkIndex);

	return true;
}

//bool ResourceUploader::UploadResource(ID3D12Resource* toUploadTo,
//	ID3D12GraphicsCommandList* commandList, void* dataPtr, size_t dataSize,
//	size_t alignment, unsigned int xOffset, unsigned int yOffset,
//	unsigned int zOffset, unsigned int subresource)
//{
//	D3D12_RESOURCE_DESC resourceDesc = toUploadTo->GetDesc();
//	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
//	UINT numRows = 0;
//	UINT64 rowSizeInBytes = 0;
//	UINT64 totalBytes = 0;
//	device->GetCopyableFootprints(&resourceDesc, subresource, 1, 0, &footprint,
//		&numRows, &rowSizeInBytes, &totalBytes);
//
//	size_t chunkIndex = uploadChunks.AllocateChunk(dataSize,
//		allocationStrategy, alignment);
//
//	if (chunkIndex == size_t(-1))
//		return false;
//
//	size_t alignedOffset = AlignAdress(
//		uploadChunks.GetStartOfChunk(chunkIndex), alignment);
//
//	if (resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER)
//	{
//		std::uint64_t sourceOffset = 0;
//		std::uint64_t destinationOffset = alignedOffset;
//		for (UINT row = 0; row < numRows; ++row)
//		{
//			std::memcpy(mappedPtr + destinationOffset,
//				static_cast<unsigned char*>(dataPtr) + sourceOffset,
//				static_cast<size_t>(rowSizeInBytes - xOffset));
//			sourceOffset += rowSizeInBytes - xOffset;
//			destinationOffset += footprint.Footprint.RowPitch;
//		}
//
//		D3D12_TEXTURE_COPY_LOCATION destination;
//		destination.pResource = toUploadTo;
//		destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
//		destination.SubresourceIndex = subresource;
//
//		D3D12_TEXTURE_COPY_LOCATION source;
//		source.pResource = buffer;
//		source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
//		source.PlacedFootprint.Offset = alignedOffset;
//		source.PlacedFootprint.Footprint.Width = footprint.Footprint.Width;
//		source.PlacedFootprint.Footprint.Height = footprint.Footprint.Height;
//		source.PlacedFootprint.Footprint.Depth = footprint.Footprint.Depth;
//		source.PlacedFootprint.Footprint.RowPitch = footprint.Footprint.RowPitch;
//		source.PlacedFootprint.Footprint.Format = resourceDesc.Format;
//
//		commandList->CopyTextureRegion(&destination, xOffset, yOffset,
//		zOffset, &source, nullptr);
//	}
//	else
//	{
//		std::memcpy(mappedPtr + alignedOffset, dataPtr, dataSize);
//		commandList->CopyBufferRegion(toUploadTo, xOffset, buffer, 
//			alignedOffset, dataSize);
//	}
//
//	return true;
//}

void ResourceUploader::RestoreUsedMemory()
{
	uploadChunks.ClearHeap();
}
