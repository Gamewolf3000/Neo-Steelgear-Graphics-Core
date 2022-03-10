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

bool BufferComponent::CreateViews(const BufferReplacementViews& replacements, const BufferHandle& handle)
{
	if (cbv.index != std::uint8_t(-1))
	{
		auto desc = CreateCBV(replacements.cb != std::nullopt ? *replacements.cb :
			cbv.desc, handle);
		if (descriptorAllocators[cbv.index].AllocateCBV(&desc) == size_t(-1))
			return false;
	}

	if (srv.index != std::uint8_t(-1))
	{
		auto desc = CreateSRV(replacements.sr != std::nullopt ? *replacements.sr :
			srv.desc, handle);
		if (descriptorAllocators[srv.index].AllocateSRV(handle.resource, &desc) ==
			size_t(-1))
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

		if (descriptorAllocators[uav.index].AllocateUAV(handle.resource, &desc,
			counterResource) == size_t(-1))
		{
			return false;
		}
	}

	if (rtv.index != std::uint8_t(-1))
	{
		auto desc = CreateRTV(replacements.rt != std::nullopt ? *replacements.rt :
			rtv.desc, handle);
		if (descriptorAllocators[rtv.index].AllocateRTV(handle.resource, &desc) ==
			size_t(-1))
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

	if (bufferInfo.heapInfo.heapType == HeapType::EXTERNAL)
	{
		auto& allocationInfo = bufferInfo.heapInfo.info.external;

		bufferAllocator.Initialize(bufferInfo.bufferInfo, device,
			bufferInfo.mappedResource, views, allocationInfo.heap,
			allocationInfo.startOffset, allocationInfo.endOffset);
	}
	else
	{
		auto& allocationInfo = bufferInfo.heapInfo.info.owned;

		bufferAllocator.Initialize(bufferInfo.bufferInfo, device,
			bufferInfo.mappedResource, views, allocationInfo.heapSize);
	}
}

void BufferComponent::InitializeDescriptorAllocators(ID3D12Device* device,
	const std::vector<DescriptorAllocationInfo<BufferViewDesc>>& descriptorInfo)
{
	for (size_t i = 0; i < descriptorInfo.size(); ++i)
	{
		auto& info = descriptorInfo[i];

		if (info.heapType == HeapType::EXTERNAL)
		{
			auto& allocationInfo = info.descriptorHeapInfo.external;

			descriptorAllocators.push_back(DescriptorAllocator());
			descriptorAllocators.back().Initialize(info.descriptorInfo,
				device, allocationInfo.heap, allocationInfo.startIndex,
				allocationInfo.nrOfDescriptors);
		}
		else
		{
			auto& allocationInfo = info.descriptorHeapInfo.owned;

			descriptorAllocators.push_back(DescriptorAllocator());
			descriptorAllocators.back().Initialize(info.descriptorInfo,
				device, allocationInfo.nrOfDescriptors);
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
	ResourceIndex toReturn = bufferAllocator.AllocateBuffer(nrOfElements);

	if (toReturn == ResourceIndex(-1))
		return toReturn;

	BufferHandle handle = bufferAllocator.GetHandle(toReturn);
	if (!CreateViews(replacementViews, handle))
	{
		bufferAllocator.DeallocateBuffer(toReturn);
		return ResourceIndex(-1);
	}

	return toReturn;
}

void BufferComponent::RemoveComponent(ResourceIndex indexToRemove)
{
	ResourceComponent::RemoveComponent(indexToRemove);
	bufferAllocator.DeallocateBuffer(indexToRemove);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapCBV(
	ResourceIndex indexOffset) const
{
	return descriptorAllocators[cbv.index].GetDescriptorHandle(indexOffset);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapSRV(
	ResourceIndex indexOffset) const
{
	return descriptorAllocators[srv.index].GetDescriptorHandle(indexOffset);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapUAV(
	ResourceIndex indexOffset) const
{
	return descriptorAllocators[uav.index].GetDescriptorHandle(indexOffset);
}

const D3D12_CPU_DESCRIPTOR_HANDLE BufferComponent::GetDescriptorHeapRTV(
	ResourceIndex indexOffset) const
{
	return descriptorAllocators[rtv.index].GetDescriptorHandle(indexOffset);
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

BufferHandle BufferComponent::GetBufferHandle(size_t index)
{
	return bufferAllocator.GetHandle(index);
}

D3D12_RESOURCE_STATES BufferComponent::GetCurrentState()
{
	return bufferAllocator.GetCurrentState();
}

D3D12_RESOURCE_BARRIER BufferComponent::CreateTransitionBarrier(D3D12_RESOURCE_STATES newState,
	D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	return bufferAllocator.CreateTransitionBarrier(newState, flag);
}

void BufferComponent::UpdateMappedBuffer(ResourceIndex index, void* data)
{
	bufferAllocator.UpdateMappedBuffer(index, data);
}
