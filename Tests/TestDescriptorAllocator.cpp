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

//void MassAllocateDescriptors(DescriptorAllocator& allocator, UINT nrOfDescriptors,
//	D3D12_DESCRIPTOR_HEAP_TYPE type, ID3D12Resource* resourceToAllocateFor, 
//	UINT startOffset = 0)
//{
//	for (UINT i = startOffset; i < startOffset + nrOfDescriptors; ++i)
//	{
//		switch (type)
//		{
//		case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
//			ASSERT_EQ(allocator.)
//			break;
//		case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
//			break;
//		case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
//			break;
//		default:
//			break;
//		}
//	}
//}

TEST(DescriptorAllocatorTest, AllocatesCorrectlySRV)
{

}