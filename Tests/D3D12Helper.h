#pragma once

#include <stdexcept>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "../Neo Steelgear Graphics Core/ResourceAllocator.h"
#include "../Neo Steelgear Graphics Core/Texture2DComponent.h"

struct SimpleCommandStructure
{
	ID3D12CommandQueue* queue = nullptr;
	ID3D12CommandAllocator* allocator = nullptr;
	ID3D12GraphicsCommandList* list = nullptr;

	~SimpleCommandStructure()
	{
		if (queue)
			queue->Release();

		if (allocator)
			allocator->Release();

		if (list)
			list->Release();
	}
};

void ThrowIfFailed(HRESULT hr, const std::exception& exception);

IDXGIAdapter* GetAdapter();

ID3D12Device* CreateDevice();

ID3D12Resource* CreateBuffer(ID3D12Device* device, UINT64 size, bool readback,
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

D3D12_RESOURCE_DESC CreateTexture2DDesc(UINT64 width, UINT height, UINT16 mips,
	UINT16 arraySize, DXGI_FORMAT format, const AllowedViews& allowedViews);

ID3D12Resource* CreateTexture2D(ID3D12Device* device, bool readback, 
	UINT64 width, UINT height, UINT16 arraySize = 1, UINT16 mipLevels = 1,
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

ID3D12Resource* CreateTexture2D(ID3D12Device* device, D3D12_RESOURCE_DESC& desc);

void CheckTextureData(ID3D12Resource* readbackBuffer,
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints,
	std::vector<UINT> rows, std::vector<UINT64> rowSizes, unsigned char* data);

std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>
CreateDescriptorAllocationInfo(size_t maxNrOfTextures, 
	const AllowedViews& allowedViews);

AllowedViews ReverseDescriptorAllocationInfo(
	const std::vector<DescriptorAllocationInfo<Texture2DViewDesc>>&
	descriptorAllocationInfo);

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE descriptorType, UINT nrOfDescriptors, bool shaderVisible);

ID3D12Heap* CreateResourceHeap(ID3D12Device* device, UINT64 heapSize, D3D12_HEAP_TYPE type, D3D12_HEAP_FLAGS flags);

ID3D12Fence* CreateFence(ID3D12Device* device, UINT64 initialValue,
	D3D12_FENCE_FLAGS flags);

bool CreateSimpleCommandStructure(SimpleCommandStructure& structure, 
	ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);

void TransitionResource(ID3D12GraphicsCommandList* list, 
	ID3D12Resource* resource, D3D12_RESOURCE_STATES currentState,
	D3D12_RESOURCE_STATES newState);

void SetIncrementedFence(UINT64& currentFenceValue, ID3D12CommandQueue* queue,
	ID3D12Fence* fence);

void FlushCommandQueue(UINT64& currentFenceValue, ID3D12CommandQueue* queue,
	ID3D12Fence* fence);

void ExecuteGraphicsCommandList(ID3D12GraphicsCommandList* list,
	ID3D12CommandQueue* queue);

void CheckResourceData(ID3D12Resource* resource, UINT subresource,
	unsigned char data[], int startIndex, int stopIndex);