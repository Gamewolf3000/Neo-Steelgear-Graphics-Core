#include "pch.h"

#include <array>
#include <utility>

#include "../Neo Steelgear Graphics Core/DescriptorAllocator.h"

#include "D3D12Helper.h"

TEST(DescriptorAllocatorTest, DefaultInitialisable)
{
	DescriptorAllocator descriotorAllocator;
}

TEST(DescriptorAllocatorTest, RuntimeInitialisable)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	std::array<D3D12_DESCRIPTOR_HEAP_TYPE, 3> heapTypes = {
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV };

	for (auto& type : heapTypes)
	{
		DescriptorInfo info;
		info.descriptorSize = device->GetDescriptorHandleIncrementSize(type);
		info.type = type;

		DescriptorAllocator descriptorAllocator1;
		descriptorAllocator1.Initialize(info, device, 1000);

		ID3D12DescriptorHeap* descriptorHeap = CreateDescriptorHeap(device,
			type, 3000, false);
		if (descriptorHeap == nullptr)
			FAIL() << "Cannot proceed with tests as a descriptor heap could not be created";

		DescriptorAllocator descriptorAllocator2;
		descriptorAllocator2.Initialize(info, device, descriptorHeap, 1000, 1000);
		descriptorHeap->Release();
	}

	device->Release();
}

enum class DescriptorType
{
	CBV,
	SRV,
	UAV,
	RTV,
	DSV
};

void MassAllocateDescriptors(DescriptorAllocator& allocator, UINT nrOfDescriptors,
	DescriptorType type, ID3D12Resource* resourceToAllocateFor,
	UINT startOffset = 0)
{
	for (UINT i = startOffset; i < startOffset + nrOfDescriptors; ++i)
	{
		switch (type)
		{
		case DescriptorType::CBV:
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbDesc;
			cbDesc.BufferLocation = resourceToAllocateFor->GetGPUVirtualAddress();
			cbDesc.SizeInBytes = 256;
			ASSERT_EQ(allocator.AllocateCBV(&cbDesc, i), i);
			break;
		case DescriptorType::SRV:
			D3D12_SHADER_RESOURCE_VIEW_DESC srDesc;
			srDesc.Format = DXGI_FORMAT_UNKNOWN;
			srDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srDesc.Buffer.FirstElement = 0;
			srDesc.Buffer.NumElements = 1;
			srDesc.Buffer.StructureByteStride = 256;
			srDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			ASSERT_EQ(allocator.AllocateSRV(resourceToAllocateFor, &srDesc, i), i);
			break;
		case DescriptorType::UAV:
			D3D12_UNORDERED_ACCESS_VIEW_DESC uaDesc;
			uaDesc.Format = DXGI_FORMAT_UNKNOWN;
			uaDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
			uaDesc.Buffer.FirstElement = 0;
			uaDesc.Buffer.NumElements = 1;
			uaDesc.Buffer.StructureByteStride = 256;
			uaDesc.Buffer.CounterOffsetInBytes = 0;
			uaDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
			ASSERT_EQ(allocator.AllocateUAV(resourceToAllocateFor, &uaDesc,
				nullptr, i), i);
			break;
		case DescriptorType::RTV:
			ASSERT_EQ(allocator.AllocateRTV(resourceToAllocateFor, nullptr, i), i);
			break;
		case DescriptorType::DSV:
			ASSERT_EQ(allocator.AllocateDSV(resourceToAllocateFor, nullptr, i), i);
			break;
		default:
			break;
		}
	}
}

TEST(DescriptorAllocatorTest, AllocatesCorrectlyCBV)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	ID3D12InfoQueue* infoQueue = nullptr;
	HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
	if (FAILED(hr))
		FAIL() << "Cannot proceed with tests as a info queue interface could not be queried";

	DescriptorInfo info;
	info.descriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	info.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	DescriptorAllocator descriptorAllocator;
	descriptorAllocator.Initialize(info, device, 1000);

	ID3D12Resource* resource = CreateBuffer(device, 256, false);
	if (resource == nullptr)
		FAIL() << "Cannot proceed with tests as resource could not be created";

	UINT64 nrOfMessagesBefore = infoQueue->GetNumMessagesAllowedByStorageFilter();
	MassAllocateDescriptors(descriptorAllocator, 1000, DescriptorType::CBV,
		resource, 0);
	UINT64 nrOfMessagesAfter = infoQueue->GetNumMessagesAllowedByStorageFilter();
	ASSERT_EQ(nrOfMessagesAfter, nrOfMessagesBefore);

	device->Release();
	infoQueue->Release();
	resource->Release();
}

TEST(DescriptorAllocatorTest, AllocatesCorrectlySRV)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	ID3D12InfoQueue* infoQueue = nullptr;
	HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
	if (FAILED(hr))
		FAIL() << "Cannot proceed with tests as a info queue interface could not be queried";

	DescriptorInfo info;
	info.descriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	info.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	DescriptorAllocator descriptorAllocator;
	descriptorAllocator.Initialize(info, device, 1000);

	ID3D12Resource* resource = CreateBuffer(device, 256, false);
	if (resource == nullptr)
		FAIL() << "Cannot proceed with tests as resource could not be created";

	UINT64 nrOfMessagesBefore = infoQueue->GetNumMessagesAllowedByStorageFilter();
	MassAllocateDescriptors(descriptorAllocator, 1000, DescriptorType::SRV,
		resource, 0);
	UINT64 nrOfMessagesAfter = infoQueue->GetNumMessagesAllowedByStorageFilter();
	ASSERT_EQ(nrOfMessagesAfter, nrOfMessagesBefore);

	device->Release();
	infoQueue->Release();
	resource->Release();
}

TEST(DescriptorAllocatorTest, AllocatesCorrectlyUAV)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	ID3D12InfoQueue* infoQueue = nullptr;
	HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
	if (FAILED(hr))
		FAIL() << "Cannot proceed with tests as a info queue interface could not be queried";

	DescriptorInfo info;
	info.descriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	info.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	DescriptorAllocator descriptorAllocator;
	descriptorAllocator.Initialize(info, device, 1000);

	ID3D12Resource* resource = CreateBuffer(device, 256, false, 
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	if (resource == nullptr)
		FAIL() << "Cannot proceed with tests as resource could not be created";

	UINT64 nrOfMessagesBefore = infoQueue->GetNumMessagesAllowedByStorageFilter();
	MassAllocateDescriptors(descriptorAllocator, 1000, DescriptorType::UAV,
		resource, 0);
	UINT64 nrOfMessagesAfter = infoQueue->GetNumMessagesAllowedByStorageFilter();
	ASSERT_EQ(nrOfMessagesAfter, nrOfMessagesBefore);

	device->Release();
	infoQueue->Release();
	resource->Release();
}

TEST(DescriptorAllocatorTest, AllocatesCorrectlyRTV)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	ID3D12InfoQueue* infoQueue = nullptr;
	HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
	if (FAILED(hr))
		FAIL() << "Cannot proceed with tests as a info queue interface could not be queried";

	DescriptorInfo info;
	info.descriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	info.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	DescriptorAllocator descriptorAllocator;
	descriptorAllocator.Initialize(info, device, 1000);

	ID3D12Resource* resource = CreateTexture2D(device, false, 256, 256, 1, 1,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	if (resource == nullptr)
		FAIL() << "Cannot proceed with tests as resource could not be created";

	UINT64 nrOfMessagesBefore = infoQueue->GetNumMessagesAllowedByStorageFilter();
	MassAllocateDescriptors(descriptorAllocator, 1000, DescriptorType::RTV,
		resource, 0);
	UINT64 nrOfMessagesAfter = infoQueue->GetNumMessagesAllowedByStorageFilter();
	ASSERT_EQ(nrOfMessagesAfter, nrOfMessagesBefore);

	device->Release();
	infoQueue->Release();
	resource->Release();
}

TEST(DescriptorAllocatorTest, ReallocatesCorrectly)
{
	ID3D12Device* device = CreateDevice();
	if (device == nullptr)
		FAIL() << "Cannot proceed with tests as a device could not be created";

	ID3D12InfoQueue* infoQueue = nullptr;
	HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
	if (FAILED(hr))
		FAIL() << "Cannot proceed with tests as a info queue interface could not be queried";

	DescriptorInfo info;
	info.descriptorSize = device->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	info.type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	DescriptorAllocator descriptorAllocator;
	descriptorAllocator.Initialize(info, device, 1000);

	ID3D12Resource* resource = CreateTexture2D(device, false, 256, 256, 1, 1,
		DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	if (resource == nullptr)
		FAIL() << "Cannot proceed with tests as resource could not be created";

	UINT64 nrOfMessagesBefore = infoQueue->GetNumMessagesAllowedByStorageFilter();
	MassAllocateDescriptors(descriptorAllocator, 1000, DescriptorType::RTV,
		resource, 0);
	UINT64 nrOfMessagesAfter = infoQueue->GetNumMessagesAllowedByStorageFilter();
	ASSERT_EQ(nrOfMessagesAfter, nrOfMessagesBefore);

	for (size_t i = 0; i < 1000; ++i)
	{
		descriptorAllocator.DeallocateDescriptor(i);
	}

	MassAllocateDescriptors(descriptorAllocator, 1000, DescriptorType::RTV,
		resource, 0);
	nrOfMessagesAfter = infoQueue->GetNumMessagesAllowedByStorageFilter();
	ASSERT_EQ(nrOfMessagesAfter, nrOfMessagesBefore);

	device->Release();
	infoQueue->Release();
	resource->Release();
}