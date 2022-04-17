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

		DescriptorAllocator descriptorAllocator;
		descriptorAllocator.Initialize(info, device, 1000);
	}

	device->Release();
}