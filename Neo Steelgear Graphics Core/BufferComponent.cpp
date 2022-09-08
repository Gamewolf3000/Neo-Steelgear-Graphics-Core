#include "BufferComponent.h"

D3D12_CONSTANT_BUFFER_VIEW_DESC BufferComponent::CreateCBV(
	const BufferViewDesc::ConstantBufferDesc& desc, const BufferHandle& handle)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC toReturn;
	toReturn.BufferLocation = handle.resource->GetGPUVirtualAddress() + 
		handle.startOffset + desc.byteOffset;
	toReturn.SizeInBytes = static_cast<UINT>(handle.nrOfElements * 
		bufferAllocator.GetElementSize() + desc.sizeModifier);

	return toReturn;
}

D3D12_SHADER_RESOURCE_VIEW_DESC BufferComponent::CreateSRV(
	const BufferViewDesc::ShaderResourceDesc& desc, const BufferHandle& handle)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC toReturn;

	toReturn.Format = DXGI_FORMAT_UNKNOWN;
	toReturn.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	toReturn.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	toReturn.Buffer.FirstElement = handle.startOffset / bufferAllocator.GetElementSize() +
		desc.firstElement;
	toReturn.Buffer.NumElements = static_cast<UINT>(desc.nrOfElements != size_t(-1) ? 
		desc.nrOfElements : handle.nrOfElements);
	toReturn.Buffer.StructureByteStride =
		static_cast<UINT>(bufferAllocator.GetElementSize());
	toReturn.Buffer.Flags = desc.flags;

	return toReturn;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC BufferComponent::CreateUAV(
	const BufferViewDesc::UnorderedAccessDesc& desc, const BufferHandle& handle)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC toReturn;

	toReturn.Format = DXGI_FORMAT_UNKNOWN;
	toReturn.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	toReturn.Buffer.FirstElement = handle.startOffset / bufferAllocator.GetElementSize() +
		desc.firstElement;
	toReturn.Buffer.NumElements = static_cast<UINT>(desc.nrOfElements != size_t(-1) ?
		desc.nrOfElements : handle.nrOfElements);
	toReturn.Buffer.StructureByteStride = 
		static_cast<UINT>(bufferAllocator.GetElementSize());
	toReturn.Buffer.CounterOffsetInBytes = desc.counterOffsetInBytes;
	toReturn.Buffer.Flags = desc.flags;

	return toReturn;
}

D3D12_RENDER_TARGET_VIEW_DESC BufferComponent::CreateRTV(
	const BufferViewDesc::RenderTargetDesc& desc, const BufferHandle& handle)
{
	D3D12_RENDER_TARGET_VIEW_DESC toReturn;

	toReturn.Format = desc.format;
	toReturn.ViewDimension = D3D12_RTV_DIMENSION_BUFFER;
	toReturn.Buffer.FirstElement = handle.startOffset + desc.byteOffset;
	toReturn.Buffer.NumElements = static_cast<UINT>(desc.nrOfElements != size_t(-1) ?
		desc.nrOfElements : handle.nrOfElements);

	return toReturn;
}

bool BufferComponent::CreateViews(const BufferReplacementViews& replacements,
	const BufferHandle& handle, ResourceIndex& resourceIndex)
{
	if (cbv.index != std::uint8_t(-1))
	{
		auto desc = CreateCBV(replacements.cb != std::nullopt ? *replacements.cb :
			cbv.desc, handle);
		resourceIndex.descriptorIndex = 
			descriptorAllocators[cbv.index].AllocateCBV(&desc);
		if (resourceIndex.descriptorIndex == size_t(-1))
		{
			return false;
		}
	}

	if (srv.index != std::uint8_t(-1))
	{
		auto desc = CreateSRV(replacements.sr != std::nullopt ? *replacements.sr :
			srv.desc, handle);
		resourceIndex.descriptorIndex = descriptorAllocators[srv.index].AllocateSRV(
			handle.resource, &desc);
		if (resourceIndex.descriptorIndex == size_t(-1))
		{
			return false;
		}
	}

	if (uav.index != std::uint8_t(-1))
	{
		auto desc = CreateUAV(replacements.ua != std::nullopt ? *replacements.ua :
			uav.desc, handle);
		ID3D12Resource* counterResource = uav.desc.counterResource;

		if (replacements.ua != std::nullopt && 
			replacements.ua->counterResource != nullptr)
		{
			counterResource = replacements.ua->counterResource;
		}

		resourceIndex.descriptorIndex = descriptorAllocators[uav.index].AllocateUAV(
			handle.resource, &desc, counterResource);
		if (resourceIndex.descriptorIndex == size_t(-1))
		{
			return false;
		}
	}

	if (rtv.index != std::uint8_t(-1))
	{
		auto desc = CreateRTV(replacements.rt != std::nullopt ? *replacements.rt :
			rtv.desc, handle);
		resourceIndex.descriptorIndex = descriptorAllocators[rtv.index].AllocateRTV(
			handle.resource, &desc);
		if (resourceIndex.descriptorIndex == size_t(-1))
		{
			return false;
		}
	}

	return true;
}

void BufferComponent::InitializeBufferAllocator(ID3D12Device* device,
	const BufferComponentInfo& bufferInfo)
{
	AllowedViews views{ srv.index != std::uint8_t(-1), uav.index != std::uint8_t(-1),
		rtv.index != std::uint8_t(-1), false };

	bufferAllocator.Initialize(bufferInfo.bufferInfo, device,
		bufferInfo.mappedResource, views, bufferInfo.memoryInfo.initialMinimumHeapSize,
		bufferInfo.memoryInfo.expansionMinimumSize, bufferInfo.memoryInfo.heapAllocator);
}

void BufferComponent::InitializeDescriptorAllocators(ID3D12Device* device,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo)
{
	for (size_t i = 0; i < descriptorInfo.size(); ++i)
	{
		auto& info = descriptorInfo[i];
		D3D12_DESCRIPTOR_HEAP_TYPE descriptorType = 
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		switch (info.viewType)
		{
		case ViewType::RTV:
			descriptorType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			break;
		case ViewType::DSV:
			descriptorType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			break;
		default:
			break;
		}

		if (info.heapType == HeapType::EXTERNAL)
		{
			auto& allocationInfo = info.descriptorHeapInfo.external;
			size_t nrOfDescriptors = allocationInfo.nrOfDescriptors;
			nrOfDescriptors = max(nrOfDescriptors, nrOfDescriptors + 1);

			descriptorAllocators.push_back(DescriptorAllocator());
			descriptorAllocators.back().Initialize(descriptorType, device,
				allocationInfo.heap, allocationInfo.startIndex, nrOfDescriptors);
		}
		else
		{
			auto& allocationInfo = info.descriptorHeapInfo.owned;
			size_t nrOfDescriptors = allocationInfo.nrOfDescriptors;
			nrOfDescriptors = max(nrOfDescriptors, nrOfDescriptors + 1);

			descriptorAllocators.push_back(DescriptorAllocator());
			descriptorAllocators.back().Initialize(descriptorType, device,
				nrOfDescriptors);
		}
	
		switch (info.viewType)
		{
		case ViewType::CBV:
			cbv.index = std::uint8_t(i);
			cbv.desc = info.viewDesc.cb;
			break;
		case ViewType::SRV:
			srv.index = std::uint8_t(i);
			srv.desc = info.viewDesc.sr;
			break;
		case ViewType::UAV:
			uav.index = std::uint8_t(i);
			uav.desc = info.viewDesc.ua;
			break;
		case ViewType::RTV:
			rtv.index = std::uint8_t(i);
			rtv.desc = info.viewDesc.rt;
			break;
		}
	}
}

BufferComponent::BufferComponent(BufferComponent&& other) noexcept :
	ResourceComponent(std::move(other)),
	bufferAllocator(std::move(other.bufferAllocator)), cbv(other.cbv), 
	srv(other.srv), uav(other.uav), rtv(other.rtv)
{
	other.cbv.index = other.srv.index = other.uav.index = 
		other.rtv.index = std::uint8_t(-1);
}

BufferComponent& BufferComponent::operator=(BufferComponent&& other) noexcept
{
	if (this != &other)
	{
		ResourceComponent::operator=(std::move(other));
		bufferAllocator = std::move(other.bufferAllocator);
		cbv = other.cbv;
		srv = other.srv;
		uav = other.uav;
		rtv = other.rtv;
		other.cbv.index = other.srv.index = other.uav.index = 
			other.rtv.index = std::uint8_t(-1);
	}

	return *this;
}

void BufferComponent::Initialize(ID3D12Device* device, 
	const BufferComponentInfo& bufferInfo, 
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo)
{
	InitializeDescriptorAllocators(device, descriptorInfo); // Must be done first so allowed views are set correct
	InitializeBufferAllocator(device, bufferInfo);
}

ResourceIndex BufferComponent::CreateBuffer(size_t nrOfElements,
	const BufferReplacementViews& replacementViews)
{
	ResourceIndex toReturn;
	toReturn.allocatorIdentifier = bufferAllocator.AllocateBuffer(nrOfElements);

	BufferHandle handle = bufferAllocator.GetHandle(toReturn.allocatorIdentifier);
	if (!CreateViews(replacementViews, handle, toReturn))
	{
		bufferAllocator.DeallocateBuffer(toReturn.allocatorIdentifier);
		throw std::runtime_error("Cannot create views for buffer resource");
	}

	return toReturn;
}

void BufferComponent::RemoveComponent(const ResourceIndex& indexToRemove)
{
	ResourceComponent::RemoveComponent(indexToRemove);
	bufferAllocator.DeallocateBuffer(indexToRemove.allocatorIdentifier);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapCBV() const
{
	return descriptorAllocators[cbv.index].GetDescriptorHandle(0);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapSRV() const
{
	return descriptorAllocators[srv.index].GetDescriptorHandle(0);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapUAV() const
{
	return descriptorAllocators[uav.index].GetDescriptorHandle(0);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapRTV() const
{
	return descriptorAllocators[rtv.index].GetDescriptorHandle(0);
}

bool BufferComponent::HasDescriptorsOfType(ViewType type) const
{
	switch (type)
	{
	case ViewType::CBV:
		return cbv.index != uint8_t(-1);
	case ViewType::SRV:
		return srv.index != uint8_t(-1);
	case ViewType::UAV:
		return uav.index != uint8_t(-1);
	case ViewType::RTV:
		return rtv.index != uint8_t(-1);
	case ViewType::DSV:
		return false;
	default:
		throw(std::runtime_error("Incorrect descriptor type when checking for descriptors"));
		break;
	}
}

BufferHandle BufferComponent::GetBufferHandle(const ResourceIndex& resourceIndex)
{
	return bufferAllocator.GetHandle(resourceIndex.allocatorIdentifier);
}

const BufferHandle BufferComponent::GetBufferHandle(const ResourceIndex& resourceIndex) const
{
	return bufferAllocator.GetHandle(resourceIndex.allocatorIdentifier);
}

unsigned char* BufferComponent::GetMappedPtr(const ResourceIndex& resourceIndex)
{
	return bufferAllocator.GetMappedPtr(resourceIndex.allocatorIdentifier);
}

D3D12_RESOURCE_STATES BufferComponent::GetCurrentState()
{
	return bufferAllocator.GetCurrentState();
}

void BufferComponent::CreateTransitionBarrier(D3D12_RESOURCE_STATES newState,
	std::vector<D3D12_RESOURCE_BARRIER>& barriers, D3D12_RESOURCE_BARRIER_FLAGS flag,
	std::optional<D3D12_RESOURCE_STATES> assumedInitialState)
{
	return bufferAllocator.CreateTransitionBarrier(newState, barriers, flag, assumedInitialState);
}

void BufferComponent::UpdateMappedBuffer(const ResourceIndex& resourceIndex, void* data)
{
	bufferAllocator.UpdateMappedBuffer(resourceIndex.allocatorIdentifier, data);
}
