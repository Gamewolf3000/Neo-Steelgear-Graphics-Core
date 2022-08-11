#pragma once

#include <array>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <optional>

#include "ResourceComponent.h"
#include "TextureAllocator.h"

struct TextureComponentInfo
{
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	std::uint8_t texelSize = 0;
	ResourceComponentMemoryInfo memoryInfo;

	TextureComponentInfo(DXGI_FORMAT format, std::uint8_t texelSize,
		const ResourceComponentMemoryInfo& memoryInfo) : format(format),
		texelSize(texelSize), memoryInfo(memoryInfo)
	{
		// Empty
	}
};

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
class TextureComponent : public ResourceComponent
{
public:
	union TextureViewDesc
	{
		DescSRV sr;
		DescUAV ua;
		DescRTV rt;
		DescDSV ds;

		TextureViewDesc(ViewType type)
		{
			if (type == ViewType::SRV)
				sr = DescSRV();
			else if (type == ViewType::UAV)
				ua = DescUAV();
			else if (type == ViewType::RTV)
				rt = DescRTV();
			else if (type == ViewType::DSV)
				ds = DescDSV();
		}
	};

	struct TextureReplacementViews
	{
		std::optional<DescSRV> sr = std::nullopt;
		std::optional<DescUAV> ua = std::nullopt;
		std::optional<DescRTV> rt = std::nullopt;
		std::optional<DescDSV> ds = std::nullopt;
	};

protected:
	DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;
	std::uint8_t texelSize = 0;

	TextureAllocator textureAllocator;

	template<typename T>
	struct SRV
	{
		std::uint8_t index = std::uint8_t(-1);
		T desc;
	};
	SRV<DescSRV> srv;

	template<typename T>
	struct UAV
	{
		std::uint8_t index = std::uint8_t(-1);
		T desc;
	};
	UAV<DescUAV> uav;

	template<typename T>
	struct RTV
	{
		std::uint8_t index = std::uint8_t(-1);
		T desc;
	};
	RTV<DescRTV> rtv;

	template<typename T>
	struct DSV
	{
		std::uint8_t index = std::uint8_t(-1);
		T desc;
	};
	DSV<DescDSV> dsv;

	virtual D3D12_SHADER_RESOURCE_VIEW_DESC CreateSRV(
		const DescSRV& desc, const TextureHandle& handle) = 0;
	virtual D3D12_UNORDERED_ACCESS_VIEW_DESC CreateUAV(
		const DescUAV& desc, const TextureHandle& handle) = 0;
	virtual D3D12_RENDER_TARGET_VIEW_DESC CreateRTV(
		const DescRTV& desc, const TextureHandle& handle) = 0;
	virtual D3D12_DEPTH_STENCIL_VIEW_DESC CreateDSV(
		const DescDSV& desc, const TextureHandle& handle) = 0;

	virtual bool CreateViews(const TextureReplacementViews& replacements, 
		const TextureHandle& handle, ResourceIndex& resourceIndex) = 0;

	void InitializeTextureAllocator(ID3D12Device* device,
		const TextureComponentInfo& textureInfo);
	void InitializeDescriptorAllocators(ID3D12Device* device,
		const std::vector<DescriptorAllocationInfo<TextureViewDesc>>& descriptorInfo);

public:
	TextureComponent() = default;
	~TextureComponent() = default;
	TextureComponent(const TextureComponent& other) = delete;
	TextureComponent& operator=(const TextureComponent& other) = delete;
	TextureComponent(TextureComponent&& other) noexcept;
	TextureComponent& operator=(TextureComponent&& other) noexcept;

	void Initialize(ID3D12Device* device, const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<TextureViewDesc>>& descriptorInfo);

	void RemoveComponent(const ResourceIndex& indexToRemove) override;

	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapSRV() const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapUAV() const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapRTV() const override;
	const D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHeapDSV() const override;
	bool HasDescriptorsOfType(ViewType type) const override;

	TextureHandle GetTextureHandle(const ResourceIndex& resourceIndex);
	const TextureHandle GetTextureHandle(const ResourceIndex& resourceIndex) const;

	D3D12_RESOURCE_BARRIER CreateTransitionBarrier(const ResourceIndex& resourceIndex,
		D3D12_RESOURCE_STATES newState,
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE);
};

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline void 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::InitializeTextureAllocator(
	ID3D12Device* device, const TextureComponentInfo& textureInfo)
{
	AllowedViews views{ srv.index != std::uint8_t(-1), uav.index != std::uint8_t(-1),
		rtv.index != std::uint8_t(-1), dsv.index != std::uint8_t(-1) };

	const auto& memoryInfo = textureInfo.memoryInfo;
	textureAllocator.Initialize(device, views, memoryInfo.initialMinimumHeapSize,
		memoryInfo.expansionMinimumSize, memoryInfo.heapAllocator);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline void
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::InitializeDescriptorAllocators(
	ID3D12Device* device, 
	const std::vector<DescriptorAllocationInfo<TextureViewDesc>>& descriptorInfo)
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
				allocationInfo.heap, allocationInfo.startIndex,
				nrOfDescriptors);
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
		case ViewType::DSV:
			dsv.index = std::uint8_t(i);
			dsv.desc = info.viewDesc.ds;
			break;
		}
	}
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::TextureComponent(
	TextureComponent&& other) noexcept : ResourceComponent(std::move(other)), 
	textureFormat(other.textureFormat), texelSize(other.texelSize),
	textureAllocator(std::move(other.textureAllocator)), srv(other.srv), 
	uav(other.uav), rtv(other.rtv), dsv(other.dsv)
{
	other.textureFormat = DXGI_FORMAT_UNKNOWN;
	other.texelSize = 0;
	other.srv.index = other.uav.index = other.rtv.index =
		other.dsv.index = std::uint8_t(-1);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>&
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::operator=(
	TextureComponent&& other) noexcept
{
	if (this != &other)
	{
		ResourceComponent::operator=(std::move(other));
		textureFormat = other.textureFormat;
		texelSize = other.texelSize;
		textureAllocator = std::move(other.textureAllocator);
		srv = other.srv;
		uav = other.uav;
		rtv = other.rtv;
		dsv = other.dsv;

		other.textureFormat = DXGI_FORMAT_UNKNOWN;
		other.texelSize = 0;
		other.srv.index = other.uav.index = other.rtv.index =
			other.dsv.index = std::uint8_t(-1);
	}

	return *this;
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline void TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::Initialize(
	ID3D12Device* device, const TextureComponentInfo& textureInfo,
	const std::vector<DescriptorAllocationInfo<TextureViewDesc>>& descriptorInfo)
{
	textureFormat = textureInfo.format;
	texelSize = textureInfo.texelSize;
	InitializeDescriptorAllocators(device, descriptorInfo); // Must be done first so allowed views are set correct
	InitializeTextureAllocator(device, textureInfo);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline void TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::RemoveComponent(
	const ResourceIndex& indexToRemove)
{
	ResourceComponent::RemoveComponent(indexToRemove);
	textureAllocator.DeallocateTexture(indexToRemove.allocatorIdentifier);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::GetDescriptorHeapSRV() const
{
	return descriptorAllocators[srv.index].GetDescriptorHandle(0);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::GetDescriptorHeapUAV() const
{
	return descriptorAllocators[uav.index].GetDescriptorHandle(0);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::GetDescriptorHeapRTV() const
{
	return descriptorAllocators[rtv.index].GetDescriptorHandle(0);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline const D3D12_CPU_DESCRIPTOR_HANDLE 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::GetDescriptorHeapDSV() const
{
	return descriptorAllocators[dsv.index].GetDescriptorHandle(0);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline bool 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::HasDescriptorsOfType(
	ViewType type) const
{
	switch (type)
	{
	case ViewType::CBV:
		return false;
	case ViewType::SRV:
		return srv.index != uint8_t(-1);
	case ViewType::UAV:
		return uav.index != uint8_t(-1);
	case ViewType::RTV:
		return rtv.index != uint8_t(-1);
	case ViewType::DSV:
		return dsv.index != uint8_t(-1);
	default:
		throw(std::runtime_error("Incorrect descriptor type when checking for descriptors"));
	}
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline TextureHandle 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::GetTextureHandle(
	const ResourceIndex& resourceIndex)
{
	return textureAllocator.GetHandle(resourceIndex.allocatorIdentifier);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline const TextureHandle TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::GetTextureHandle(const ResourceIndex& resourceIndex) const
{
	return textureAllocator.GetHandle(resourceIndex.allocatorIdentifier);
}

template<typename DescSRV, typename DescUAV, typename DescRTV, typename DescDSV>
inline D3D12_RESOURCE_BARRIER 
TextureComponent<DescSRV, DescUAV, DescRTV, DescDSV>::CreateTransitionBarrier(
	const ResourceIndex& resourceIndex, D3D12_RESOURCE_STATES newState,
	D3D12_RESOURCE_BARRIER_FLAGS flag)
{
	return textureAllocator.CreateTransitionBarrier(
		resourceIndex.allocatorIdentifier, newState, flag);
}
