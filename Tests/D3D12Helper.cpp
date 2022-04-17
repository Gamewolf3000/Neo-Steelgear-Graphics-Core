#include "pch.h"
#include "D3D12Helper.h"

#include "gtest\gtest.h"

void ThrowIfFailed(HRESULT hr, const std::exception& exception)
{
	if (FAILED(hr))
		throw exception;
}

IDXGIAdapter* GetAdapter()
{
	IDXGIFactory* factory;
	HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
	ThrowIfFailed(hr, std::runtime_error("Could not create dxgi factory"));

	unsigned int adapterIndex = 0;
	IDXGIAdapter* toReturn = nullptr;
	size_t adapterVideoMemory = 0;
	hr = S_OK;

	while (hr == S_OK)
	{
		IDXGIAdapter* currentAdapter = nullptr;
		hr = factory->EnumAdapters(adapterIndex, &currentAdapter);
		if (FAILED(hr))
			break; // No more adapters

		hr = D3D12CreateDevice(currentAdapter, D3D_FEATURE_LEVEL_12_0,
			__uuidof(ID3D12Device), nullptr);
		if (FAILED(hr))
		{
			currentAdapter->Release(); // Adapter did not support feature level 12.0
		}
		else
		{
			DXGI_ADAPTER_DESC desc;
			currentAdapter->GetDesc(&desc);

			if (desc.DedicatedVideoMemory > adapterVideoMemory)
			{
				toReturn = currentAdapter;
				adapterVideoMemory = desc.DedicatedVideoMemory;
			}
			else
			{
				currentAdapter->Release();
			}
		}

		++adapterIndex;
	}

	return toReturn;
}

void EnableDebugLayer()
{
	ID3D12Debug* debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
		std::runtime_error("Could not get debug interface"));
	debugController->EnableDebugLayer();
	debugController->Release();
}

void EnableGPUBasedValidation()
{
	ID3D12Debug1* debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)),
		std::runtime_error("Could not get debug interface"));
	debugController->SetEnableGPUBasedValidation(true);
	debugController->Release();
}

ID3D12Device* CreateDevice()
{
	EnableDebugLayer();
	EnableGPUBasedValidation();

	IDXGIAdapter* adapter = GetAdapter();
	if (adapter == nullptr)
		return nullptr;

	ID3D12Device* toReturn = nullptr;
	HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0,
		IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12Resource* CreateBuffer(ID3D12Device* device, UINT64 size,
	bool readback)
{
	D3D12_RESOURCE_DESC desc;
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Width = size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProperties;
	ZeroMemory(&heapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	heapProperties.Type = readback ? D3D12_HEAP_TYPE_READBACK :
		D3D12_HEAP_TYPE_DEFAULT;

	ID3D12Resource* toReturn = nullptr;
	HRESULT hr = device->CreateCommittedResource(&heapProperties,
		D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12Resource* CreateTexture2D(ID3D12Device* device, bool readback,
	UINT64 width, UINT height, UINT16 arraySize = 1, UINT16 mipLevels = 1,
	DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM)
{
	D3D12_RESOURCE_DESC resourceDesc;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.DepthOrArraySize = arraySize;
	resourceDesc.MipLevels = mipLevels;
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heapProperties;
	ZeroMemory(&heapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	heapProperties.Type = readback ? D3D12_HEAP_TYPE_READBACK :
		D3D12_HEAP_TYPE_DEFAULT;

	ID3D12Resource* toReturn = nullptr;
	HRESULT hr = device->CreateCommittedResource(&heapProperties,
		D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12Resource* CreateTexture2D(ID3D12Device* device, D3D12_RESOURCE_DESC& desc)
{
	D3D12_HEAP_PROPERTIES heapProperties;
	ZeroMemory(&heapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	ID3D12Resource* toReturn = nullptr;
	HRESULT hr = device->CreateCommittedResource(&heapProperties,
		D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12Fence* CreateFence(ID3D12Device* device, UINT64 initialValue,
	D3D12_FENCE_FLAGS flags)
{
	ID3D12Fence* toReturn;

	HRESULT hr = device->CreateFence(initialValue, flags, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12CommandQueue* CreateCommandQueue(ID3D12Device* device,
	const D3D12_COMMAND_QUEUE_DESC& desc)
{
	ID3D12CommandQueue* toReturn;

	HRESULT hr = device->CreateCommandQueue(&desc, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12CommandAllocator* CreateCommandAllocator(ID3D12Device* device,
	D3D12_COMMAND_LIST_TYPE type)
{
	ID3D12CommandAllocator* toReturn;

	HRESULT hr = device->CreateCommandAllocator(type, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return toReturn;
}

ID3D12GraphicsCommandList* CreateCommandList(ID3D12Device* device,
	D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* allocator)
{
	ID3D12CommandList* toReturn;

	HRESULT hr = device->CreateCommandList(0, type, allocator,
		nullptr, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		return nullptr;

	return static_cast<ID3D12GraphicsCommandList*>(toReturn);
}

bool CreateSimpleCommandStructure(SimpleCommandStructure& structure,
	ID3D12Device* device,
	D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC desc;
	ZeroMemory(&desc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	desc.Type = type;

	structure.queue = CreateCommandQueue(device, desc);
	structure.allocator = CreateCommandAllocator(device, type);
	structure.list = CreateCommandList(device, type, structure.allocator);

	return !(structure.queue == nullptr ||
		structure.allocator == nullptr || structure.list == nullptr);
}

void TransitionResource(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
	D3D12_RESOURCE_STATES currentState, D3D12_RESOURCE_STATES newState)
{
	D3D12_RESOURCE_BARRIER barrier;
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = currentState;
	barrier.Transition.StateAfter = newState;

	list->ResourceBarrier(1, &barrier);
}

void SetIncrementedFence(UINT64& currentFenceValue, ID3D12CommandQueue* queue, ID3D12Fence* fence)
{
	++currentFenceValue;
	ThrowIfFailed(queue->Signal(fence, currentFenceValue), std::runtime_error("Error setting new fence value"));
}

void WaitForFenceCPU(UINT64 currentFenceValue, ID3D12Fence* fence)
{
	if (fence->GetCompletedValue() < currentFenceValue)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, 0, 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(fence->SetEventOnCompletion(currentFenceValue, eventHandle),
			std::runtime_error("Error setting wait for fence event"));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void FlushCommandQueue(UINT64& currentFenceValue, ID3D12CommandQueue* queue, ID3D12Fence* fence)
{
	SetIncrementedFence(currentFenceValue, queue, fence);
	WaitForFenceCPU(currentFenceValue, fence);
}

void ExecuteGraphicsCommandList(ID3D12GraphicsCommandList* list, ID3D12CommandQueue* queue)
{
	HRESULT hr = list->Close();
	auto errorInfo = std::runtime_error("Error: Could not close command list");
	ThrowIfFailed(hr, errorInfo);
	ID3D12CommandList* temp = list;
	queue->ExecuteCommandLists(1, &temp);
}