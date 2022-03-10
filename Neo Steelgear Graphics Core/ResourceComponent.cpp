#include "ResourceComponent.h"

ResourceComponent::ResourceComponent(ResourceComponent&& other) noexcept : 
	descriptorAllocators(std::move(other.descriptorAllocators))
{
	// Empty
}

ResourceComponent& ResourceComponent::operator=(ResourceComponent&& other) noexcept
{
	if (this != &other)
		descriptorAllocators = std::move(other.descriptorAllocators);

	return *this;
}

void ResourceComponent::RemoveComponent(ResourceIndex indexToRemove)
{
	for (auto& descriptorAllocator : descriptorAllocators)
		descriptorAllocator.DeallocateDescriptor(indexToRemove);
}

const D3D12_CPU_DESCRIPTOR_HANDLE ResourceComponent::GetDescriptorHeapCBV(
	ResourceIndex indexOffset) const
{
	(void)indexOffset;
	throw std::runtime_error("Attempting to fetch CBV heap from incompatible component");
}

const D3D12_CPU_DESCRIPTOR_HANDLE ResourceComponent::GetDescriptorHeapSRV(
	ResourceIndex indexOffset) const
{
	(void)indexOffset;
	throw std::runtime_error("Attempting to fetch SRV heap from incompatible component");
}

const D3D12_CPU_DESCRIPTOR_HANDLE ResourceComponent::GetDescriptorHeapUAV(
	ResourceIndex indexOffset) const
{
	(void)indexOffset;
	throw std::runtime_error("Attempting to fetch UAV heap from incompatible component");
}

const D3D12_CPU_DESCRIPTOR_HANDLE ResourceComponent::GetDescriptorHeapRTV(
	ResourceIndex indexOffset) const
{
	(void)indexOffset;
	throw std::runtime_error("Attempting to fetch RTV heap from incompatible component");
}

const D3D12_CPU_DESCRIPTOR_HANDLE ResourceComponent::GetDescriptorHeapDSV(
	ResourceIndex indexOffset) const
{
	(void)indexOffset;
	throw std::runtime_error("Attempting to fetch DSV heap from incompatible component");
}

size_t ResourceComponent::NrOfDescriptors() const
{
	if (descriptorAllocators.size() == 0)
		return 0;

	return descriptorAllocators[0].NrOfStoredDescriptors(); // All have the same number
}
