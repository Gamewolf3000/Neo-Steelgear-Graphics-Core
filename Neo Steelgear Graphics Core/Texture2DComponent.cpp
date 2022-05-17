#include "Texture2DComponent.h"

D3D12_SHADER_RESOURCE_VIEW_DESC Texture2DComponent::CreateSRV(
	const Texture2DShaderResourceDesc& desc, const TextureHandle& handle)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC toReturn;

	toReturn.Format = desc.viewFormat == DXGI_FORMAT_UNKNOWN ? 
		textureAllocator.GetTextureFormat() : desc.viewFormat;
	toReturn.Shader4ComponentMapping = desc.componentMapping;

	if (handle.dimensions.depthOrArraySize == 1)
	{
		toReturn.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		toReturn.Texture2D.MostDetailedMip = desc.mostDetailedMip;
		toReturn.Texture2D.MipLevels = desc.mipLevels;
		toReturn.Texture2D.PlaneSlice = desc.planeSlice;
		toReturn.Texture2D.ResourceMinLODClamp = desc.resourceMinLODClamp;
	}
	else
	{
		toReturn.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		toReturn.Texture2DArray.MostDetailedMip = desc.mostDetailedMip;
		toReturn.Texture2DArray.MipLevels = desc.mipLevels;
		toReturn.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
		unsigned int arraySize = static_cast<unsigned int>(
			min(handle.dimensions.depthOrArraySize, desc.arraySize));
		toReturn.Texture2DArray.ArraySize = arraySize;
		toReturn.Texture2DArray.PlaneSlice = desc.planeSlice;
		toReturn.Texture2DArray.ResourceMinLODClamp = desc.resourceMinLODClamp;
	}

	return toReturn;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC Texture2DComponent::CreateUAV(
	const Texture2DUnorderedAccessDesc& desc, const TextureHandle& handle)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC toReturn;

	toReturn.Format = desc.viewFormat == DXGI_FORMAT_UNKNOWN ?
		textureAllocator.GetTextureFormat() : desc.viewFormat;
	
	if (handle.dimensions.depthOrArraySize == 1)
	{
		toReturn.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		toReturn.Texture2D.MipSlice = desc.mipSlice;
		toReturn.Texture2D.PlaneSlice = desc.planeSlice;
	}
	else
	{
		toReturn.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		toReturn.Texture2DArray.MipSlice = desc.mipSlice;
		toReturn.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
		unsigned int arraySize = static_cast<unsigned int>(
			min(handle.dimensions.depthOrArraySize, desc.arraySize));
		toReturn.Texture2DArray.ArraySize = arraySize;
		toReturn.Texture2DArray.PlaneSlice = desc.planeSlice;
	}

	return toReturn;
}

D3D12_RENDER_TARGET_VIEW_DESC Texture2DComponent::CreateRTV(
	const Texture2DRenderTargetDesc& desc, const TextureHandle& handle)
{
	D3D12_RENDER_TARGET_VIEW_DESC toReturn;

	toReturn.Format = desc.viewFormat == DXGI_FORMAT_UNKNOWN ?
		textureAllocator.GetTextureFormat() : desc.viewFormat;
	
	if (handle.dimensions.depthOrArraySize == 1)
	{
		toReturn.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		toReturn.Texture2D.MipSlice = desc.mipSlice;
		toReturn.Texture2D.PlaneSlice = desc.planeSlice;
	}
	else
	{
		toReturn.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		toReturn.Texture2DArray.MipSlice = desc.mipSlice;
		toReturn.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
		unsigned int arraySize = static_cast<unsigned int>(
			min(handle.dimensions.depthOrArraySize, desc.arraySize));
		toReturn.Texture2DArray.ArraySize = arraySize;
		toReturn.Texture2DArray.PlaneSlice = desc.planeSlice;
	}

	return toReturn;
}

D3D12_DEPTH_STENCIL_VIEW_DESC Texture2DComponent::CreateDSV(
	const Texture2DDepthStencilDesc& desc, const TextureHandle& handle)
{
	D3D12_DEPTH_STENCIL_VIEW_DESC toReturn;

	toReturn.Format = desc.viewFormat == DXGI_FORMAT_UNKNOWN ?
		textureAllocator.GetTextureFormat() : desc.viewFormat;
	toReturn.Flags = desc.flags;

	if (handle.dimensions.depthOrArraySize == 1)
	{
		toReturn.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		toReturn.Texture2D.MipSlice = desc.mipSlice;
	}
	else
	{
		toReturn.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		toReturn.Texture2DArray.MipSlice = desc.mipSlice;
		toReturn.Texture2DArray.FirstArraySlice = desc.firstArraySlice;
		unsigned int arraySize = static_cast<unsigned int>(
			min(handle.dimensions.depthOrArraySize, desc.arraySize));
		toReturn.Texture2DArray.ArraySize = arraySize;
	}

	return toReturn;
}

bool Texture2DComponent::CreateViews(
	const Texture2DComponentTemplate::TextureReplacementViews& replacements,
	const TextureHandle& handle)
{
	if (srv.index != std::uint8_t(-1))
	{
		auto desc = CreateSRV(replacements.sr != std::nullopt ? *replacements.sr :
			srv.desc, handle);
		if (descriptorAllocators[srv.index].AllocateSRV(
			handle.resource, &desc) == size_t(-1))
		{
			return false;
		}
	}

	if (uav.index != std::uint8_t(-1))
	{
		auto desc = CreateUAV(replacements.ua != std::nullopt ? *replacements.ua :
			uav.desc, handle);

		if (descriptorAllocators[uav.index].AllocateUAV(
			handle.resource, &desc, nullptr) == size_t(-1))
		{
			return false;
		}
	}

	if (rtv.index != std::uint8_t(-1))
	{
		auto desc = CreateRTV(replacements.rt != std::nullopt ? *replacements.rt :
			rtv.desc, handle);
		if (descriptorAllocators[rtv.index].AllocateRTV(
			handle.resource, &desc) == size_t(-1))
		{
			return false;
		}
	}

	if (dsv.index != std::uint8_t(-1))
	{
		auto desc = CreateDSV(replacements.ds != std::nullopt ? *replacements.ds :
			dsv.desc, handle);
		if (descriptorAllocators[dsv.index].AllocateDSV(
			handle.resource, &desc) == size_t(-1))
		{
			return false;
		}
	}

	return true;
}

Texture2DComponent::Texture2DComponent(Texture2DComponent&& other) noexcept : 
	TextureComponent(std::move(other))
{
	// Empty
}

Texture2DComponent& Texture2DComponent::operator=(Texture2DComponent&& other) noexcept
{
	if (this != &other)
		TextureComponent::operator=(std::move(other));

	return *this;
}

void Texture2DComponent::Initialize(ID3D12Device* deviceToUse,
	const TextureComponentInfo& textureInfo,
	const std::vector<DescriptorAllocationInfo<TextureViewDesc>>& descriptorInfo)
{
	TextureComponent::Initialize(deviceToUse, textureInfo, descriptorInfo);
}

ResourceIndex Texture2DComponent::CreateTexture(
	const TextureAllocationInfo& textureData,
	const TextureComponent<Texture2DShaderResourceDesc, 
	Texture2DUnorderedAccessDesc, Texture2DRenderTargetDesc,
	Texture2DDepthStencilDesc>::TextureReplacementViews& replacementViews)
{
	ResourceIndex toReturn = textureAllocator.AllocateTexture(textureData);

	if (toReturn == ResourceIndex(-1))
		return toReturn;

	TextureHandle handle = textureAllocator.GetHandle(toReturn);
	if (!CreateViews(replacementViews, handle))
	{
		textureAllocator.DeallocateTexture(toReturn);
		return ResourceIndex(-1);
	}

	return toReturn;
}

D3D12_RESOURCE_STATES Texture2DComponent::GetCurrentState(
	ResourceIndex resourceIndex)
{
	return textureAllocator.GetCurrentState(resourceIndex);
}

D3D12_RESOURCE_BARRIER Texture2DComponent::CreateTransitionBarrier(
	ResourceIndex resourceIndex, D3D12_RESOURCE_STATES newState, 
	D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	return textureAllocator.CreateTransitionBarrier(resourceIndex, newState, flag);
}
