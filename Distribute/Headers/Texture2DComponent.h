#pragma once

#include "TextureComponent.h"

struct Texture2DShaderResourceDesc
{
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;

	unsigned int componentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	unsigned int mostDetailedMip = 0;
	unsigned int mipLevels = static_cast<unsigned int>(-1);
	unsigned int planeSlice = 0;
	float resourceMinLODClamp = 0.0f;

	unsigned int firstArraySlice = 0;
	unsigned int arraySize = static_cast<unsigned int>(-1);
	
	bool isTextureCube = false;
};

struct Texture2DUnorderedAccessDesc
{
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;

	unsigned int mipSlice = 0;
	unsigned int planeSlice = 0;

	unsigned int firstArraySlice = 0;
	unsigned int arraySize = static_cast<unsigned int>(-1);
};

struct Texture2DRenderTargetDesc
{
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;

	unsigned int mipSlice = 0;
	unsigned int planeSlice = 0;

	unsigned int firstArraySlice = 0;
	unsigned int arraySize = static_cast<unsigned int>(-1);
};

struct Texture2DDepthStencilDesc
{
	DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN;

	D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE;
	unsigned int mipSlice = 0;

	unsigned int firstArraySlice = 0;
	unsigned int arraySize = static_cast<unsigned int>(-1);
};

typedef TextureComponent<Texture2DShaderResourceDesc, Texture2DUnorderedAccessDesc,
	Texture2DRenderTargetDesc, Texture2DDepthStencilDesc> Texture2DComponentTemplate;
typedef Texture2DComponentTemplate::TextureViewDesc Texture2DViewDesc;

class Texture2DComponent : public Texture2DComponentTemplate
{
private:
	D3D12_SHADER_RESOURCE_VIEW_DESC CreateSRV(
		const Texture2DShaderResourceDesc& desc, const TextureHandle& handle);
	D3D12_UNORDERED_ACCESS_VIEW_DESC CreateUAV(
		const Texture2DUnorderedAccessDesc& desc, const TextureHandle& handle);
	D3D12_RENDER_TARGET_VIEW_DESC CreateRTV(
		const Texture2DRenderTargetDesc& desc, const TextureHandle& handle);
	D3D12_DEPTH_STENCIL_VIEW_DESC CreateDSV(
		const Texture2DDepthStencilDesc& desc, const TextureHandle& handle);

	bool CreateViews(
		const Texture2DComponentTemplate::TextureReplacementViews& replacements,
		const TextureHandle& handle, ResourceIndex& resourceIndex);
	
public:
	Texture2DComponent() = default;
	~Texture2DComponent() = default;
	Texture2DComponent(const Texture2DComponent& other) = delete;
	Texture2DComponent& operator=(const Texture2DComponent& other) = delete;
	Texture2DComponent(Texture2DComponent&& other) noexcept;
	Texture2DComponent& operator=(Texture2DComponent&& other) noexcept;

	void Initialize(ID3D12Device* deviceToUse, const TextureComponentInfo& textureInfo,
		const std::vector<DescriptorAllocationInfo<TextureViewDesc>>& descriptorInfo);

	ResourceIndex CreateTexture(size_t width, size_t height, size_t arraySize = 1,
		size_t mipLevels = 1, std::uint8_t sampleCount = 1,
		std::uint8_t sampleQuality = 0, D3D12_CLEAR_VALUE* clearValue = nullptr,
		const TextureComponent<Texture2DShaderResourceDesc, 
		Texture2DUnorderedAccessDesc, Texture2DRenderTargetDesc, 
		Texture2DDepthStencilDesc>::TextureReplacementViews& replacementViews = {});

	D3D12_RESOURCE_STATES GetCurrentState(const ResourceIndex& resourceIndex);
	D3D12_RESOURCE_BARRIER CreateTransitionBarrier(const ResourceIndex& resourceIndex,
		D3D12_RESOURCE_STATES newState,
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		std::optional<D3D12_RESOURCE_STATES> assumedInitialState = std::nullopt);
	void TransitionAllTextures(std::vector<D3D12_RESOURCE_BARRIER>& barriers,
		D3D12_RESOURCE_STATES newState, 
		D3D12_RESOURCE_BARRIER_FLAGS flag = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		std::optional<D3D12_RESOURCE_STATES> assumedInitialState = std::nullopt);
};