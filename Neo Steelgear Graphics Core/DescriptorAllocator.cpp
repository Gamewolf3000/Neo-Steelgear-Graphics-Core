#include "DescriptorAllocator.h"

#include <stdexcept>

ID3D12DescriptorHeap* DescriptorAllocator::AllocateHeap(size_t nrOfDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;

	desc.Type = heapData.descriptorInfo.type;
	desc.NumDescriptors = static_cast<UINT>(nrOfDescriptors);
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Not shader visible
	desc.NodeMask = 0;

	ID3D12DescriptorHeap* toReturn;
	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		throw std::runtime_error("Error: Could not create descriptor heap");

	return toReturn;
}

size_t DescriptorAllocator::GetFreeDescriptorIndex()
{
	if (descriptors.ActiveSize() >= heapData.endIndex - heapData.startIndex)
		return size_t(-1);

	size_t index = descriptors.Add(StoredDescriptor());
	
	return index;
}

bool DescriptorAllocator::AllocationHelper(size_t& index,
	D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
	index = GetFreeDescriptorIndex();

	if (index == size_t(-1))
		return false;

	handle = heapData.heap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += heapData.descriptorInfo.descriptorSize * 
		(heapData.startIndex + index);

	return true;
}

DescriptorAllocator::~DescriptorAllocator()
{
	if (heapData.heapOwned == true && heapData.heap != nullptr)
		heapData.heap->Release();
}

DescriptorAllocator::DescriptorAllocator(DescriptorAllocator&& other) noexcept :
	heapData(other.heapData), device(other.device), 
	descriptors(std::move(other.descriptors))
{
	other.heapData = DescriptorHeapData();
	other.device = nullptr;
}

DescriptorAllocator& 
DescriptorAllocator::operator=(DescriptorAllocator&& other) noexcept
{
	if (this != &other)
	{
		heapData = other.heapData;
		other.heapData = DescriptorHeapData();
		device = other.device;
		other.device = nullptr;
		descriptors = std::move(descriptors);
	}

	return *this;
}

void DescriptorAllocator::Initialize(const DescriptorInfo& descriptorInfo,
	ID3D12Device* deviceToUse, ID3D12DescriptorHeap* heap, size_t startIndex,
	size_t nrOfDescriptors)
{
	device = deviceToUse;
	heapData.heapOwned = false;
	heapData.descriptorInfo = descriptorInfo;
	heapData.startIndex = startIndex;
	heapData.endIndex = startIndex + nrOfDescriptors;
	heapData.heap = heap;
}

void DescriptorAllocator::Initialize(const DescriptorInfo& descriptorInfo,
	ID3D12Device* deviceToUse, size_t nrOfDescriptors)
{
	device = deviceToUse;
	heapData.heapOwned = true;
	heapData.descriptorInfo = descriptorInfo;
	heapData.startIndex = 0;
	heapData.endIndex = nrOfDescriptors;
	heapData.heap = AllocateHeap(nrOfDescriptors);
}

size_t DescriptorAllocator::AllocateSRV(ID3D12Resource* resource, 
	D3D12_SHADER_RESOURCE_VIEW_DESC* desc)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle))
		device->CreateShaderResourceView(resource, desc, handle);

	return index;
}

size_t DescriptorAllocator::AllocateDSV(ID3D12Resource* resource, 
	D3D12_DEPTH_STENCIL_VIEW_DESC* desc)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if(AllocationHelper(index, handle))
		device->CreateDepthStencilView(resource, desc, handle);

	return index;
}

size_t DescriptorAllocator::AllocateRTV(ID3D12Resource* resource, 
	D3D12_RENDER_TARGET_VIEW_DESC* desc)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle))
		device->CreateRenderTargetView(resource, desc, handle);

	return index;
}

size_t DescriptorAllocator::AllocateUAV(ID3D12Resource* resource, 
	D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, ID3D12Resource* counterResource)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle))
		device->CreateUnorderedAccessView(resource, counterResource, desc, handle);

	return index;
}

size_t DescriptorAllocator::AllocateCBV(D3D12_CONSTANT_BUFFER_VIEW_DESC* desc)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle))
		device->CreateConstantBufferView(desc, handle);

	return index;
}

void DescriptorAllocator::DeallocateDescriptor(size_t index)
{
	descriptors.Remove(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetDescriptorHandle(
	size_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE toReturn;
	toReturn = heapData.heap->GetCPUDescriptorHandleForHeapStart();

	toReturn.ptr += heapData.descriptorInfo.descriptorSize * 
		(heapData.startIndex + index);

	return toReturn;
}

size_t DescriptorAllocator::NrOfStoredDescriptors()
{
	return descriptors.TotalSize();
}
