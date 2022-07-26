#include "DescriptorAllocator.h"

#include <stdexcept>

ID3D12DescriptorHeap* DescriptorAllocator::AllocateHeap(size_t nrOfDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;

	desc.Type = heapData.descriptorType;
	desc.NumDescriptors = static_cast<UINT>(nrOfDescriptors);
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // Not shader visible
	desc.NodeMask = 0;

	ID3D12DescriptorHeap* toReturn;
	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&toReturn));

	if (FAILED(hr))
		throw std::runtime_error("Error: Could not create descriptor heap");

	return toReturn;
}

size_t DescriptorAllocator::GetFreeDescriptorIndex(size_t indexInHeap)
{
	if (descriptors.ActiveSize() >= heapData.endIndex - heapData.startIndex ||
		(indexInHeap != size_t(-1) && indexInHeap >= heapData.endIndex - heapData.startIndex))
	{
		return size_t(-1);
	}

	size_t index = size_t(-1);

	if (indexInHeap == size_t(-1))
	{
		index = descriptors.Add(StoredDescriptor());
	}
	else
	{
		descriptors.Expand(indexInHeap + 1); // Will guarantee that the index exists
		index = descriptors.AddAt(StoredDescriptor(), indexInHeap);
	}
	
	return index;
}

bool DescriptorAllocator::AllocationHelper(size_t& index,
	D3D12_CPU_DESCRIPTOR_HANDLE& handle, size_t indexInHeap)
{
	index = GetFreeDescriptorIndex(indexInHeap);

	if (index == size_t(-1))
	{
		if (heapData.heapOwned)
		{
			size_t newSize = heapData.endIndex * 2;
			ID3D12DescriptorHeap* newHeap = AllocateHeap(newSize);
			device->CopyDescriptorsSimple(static_cast<UINT>(heapData.endIndex),
				newHeap->GetCPUDescriptorHandleForHeapStart(),
				heapData.heap->GetCPUDescriptorHandleForHeapStart(),
				heapData.descriptorType);
			heapData.heap->Release();
			heapData.heap = newHeap;
			heapData.endIndex = newSize;

			index = GetFreeDescriptorIndex(indexInHeap);

			if (index == size_t(-1))
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}
		
	handle = heapData.heap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += heapData.descriptorSize * (heapData.startIndex + index);

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

void DescriptorAllocator::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE descriptorType,
	ID3D12Device* deviceToUse, ID3D12DescriptorHeap* heap, size_t startIndex,
	size_t nrOfDescriptors)
{
	device = deviceToUse;

	heapData.heapOwned = false;
	heapData.descriptorType = descriptorType;
	heapData.descriptorSize = device->GetDescriptorHandleIncrementSize(descriptorType);
	heapData.startIndex = startIndex;
	heapData.endIndex = startIndex + nrOfDescriptors;
	heapData.heap = heap;
}

void DescriptorAllocator::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE descriptorType,
	ID3D12Device* deviceToUse, size_t startNrOfDescriptors)
{
	device = deviceToUse;
	heapData.heapOwned = true;
	heapData.descriptorType = descriptorType;
	heapData.descriptorSize = device->GetDescriptorHandleIncrementSize(descriptorType);
	heapData.startIndex = 0;
	heapData.endIndex = startNrOfDescriptors;
	heapData.heap = AllocateHeap(startNrOfDescriptors);
}

size_t DescriptorAllocator::AllocateSRV(ID3D12Resource* resource, 
	const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, size_t indexInHeap)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle, indexInHeap))
	{
		device->CreateShaderResourceView(resource, desc, handle);
		descriptors[index].type = DescriptorType::SRV;
		descriptors[index].description.srv = desc != nullptr ?
			std::make_optional(*desc) : std::nullopt;
	}

	return index;
}

size_t DescriptorAllocator::AllocateDSV(ID3D12Resource* resource, 
	const D3D12_DEPTH_STENCIL_VIEW_DESC* desc, size_t indexInHeap)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if(AllocationHelper(index, handle, indexInHeap))
	{
		device->CreateDepthStencilView(resource, desc, handle);
		descriptors[index].type = DescriptorType::DSV;
		descriptors[index].description.dsv = desc != nullptr ?
			std::make_optional(*desc) : std::nullopt;
	}

	return index;
}

size_t DescriptorAllocator::AllocateRTV(ID3D12Resource* resource, 
	const D3D12_RENDER_TARGET_VIEW_DESC* desc, size_t indexInHeap)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle, indexInHeap))
	{
		device->CreateRenderTargetView(resource, desc, handle);
		descriptors[index].type = DescriptorType::RTV;
		descriptors[index].description.rtv = desc != nullptr ?
			std::make_optional(*desc) : std::nullopt;
	}

	return index;
}

size_t DescriptorAllocator::AllocateUAV(ID3D12Resource* resource, 
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc, ID3D12Resource* counterResource,
	size_t indexInHeap)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle, indexInHeap))
	{
		device->CreateUnorderedAccessView(resource, counterResource, desc, handle);
		descriptors[index].type = DescriptorType::UAV;
		descriptors[index].description.uav = desc != nullptr ?
			std::make_optional(*desc) : std::nullopt;
	}

	return index;
}

size_t DescriptorAllocator::AllocateCBV(
	const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc, size_t indexInHeap)
{
	size_t index = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE handle;

	if (AllocationHelper(index, handle, indexInHeap))
	{
		device->CreateConstantBufferView(desc, handle);
		descriptors[index].type = DescriptorType::CBV;
		descriptors[index].description.cbv = desc != nullptr ?
			std::make_optional(*desc) : std::nullopt;
	}

	return index;
}

void DescriptorAllocator::ReallocateView(size_t indexInHeap, ID3D12Resource* resource)
{
	StoredDescriptor& descriptorInfo = descriptors[indexInHeap];
	D3D12_CPU_DESCRIPTOR_HANDLE handle = 
		heapData.heap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += heapData.descriptorSize * (heapData.startIndex + indexInHeap);

	switch (descriptorInfo.type)
	{
	case DescriptorAllocator::DescriptorType::CBV:
		(*descriptorInfo.description.cbv).BufferLocation = 
			resource->GetGPUVirtualAddress();
		device->CreateConstantBufferView(&(*descriptorInfo.description.cbv), handle);
		break;
	case DescriptorAllocator::DescriptorType::SRV:
		device->CreateShaderResourceView(resource, descriptorInfo.description.srv.has_value() ?
			&(*descriptorInfo.description.srv) : nullptr, handle);
		break;
	case DescriptorAllocator::DescriptorType::UAV:
		device->CreateUnorderedAccessView(resource, nullptr, descriptorInfo.description.uav.has_value() ?
			&(*descriptorInfo.description.uav) : nullptr, handle);
		break;
	case DescriptorAllocator::DescriptorType::RTV:
		device->CreateRenderTargetView(resource, descriptorInfo.description.rtv.has_value() ?
			&(*descriptorInfo.description.rtv) : nullptr, handle);
		break;
	case DescriptorAllocator::DescriptorType::DSV:
		device->CreateShaderResourceView(resource, descriptorInfo.description.srv.has_value() ?
			&(*descriptorInfo.description.srv) : nullptr, handle);
		break;
	default:
		break;
	}
}

void DescriptorAllocator::DeallocateDescriptor(size_t index)
{
	descriptors.Remove(index);
}

const D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetDescriptorHandle(
	size_t index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE toReturn;
	toReturn = heapData.heap->GetCPUDescriptorHandleForHeapStart();

	toReturn.ptr += heapData.descriptorSize * (heapData.startIndex + index);

	return toReturn;
}

size_t DescriptorAllocator::NrOfStoredDescriptors() const
{
	return descriptors.TotalSize();
}

void DescriptorAllocator::Reset()
{
	descriptors.Clear();
}
