#pragma once
#include "D3D12/WIPD3D12.h"
#include <vector>
#include <memory>


namespace WIP3D
{
	class Resource;
	class Texture;
	class Buffer;
	using ResourceWeakPtr = std::weak_ptr<Resource>;
	using ConstTextureSharedPtrRef = const std::shared_ptr<Texture>&;
	using ConstBufferSharedPtrRef = const std::shared_ptr<Buffer>&;
	struct ResourceViewInfo
	{
		ResourceViewInfo() = default;
		ResourceViewInfo(uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
			: mostDetailedMip(mostDetailedMip), mipCount(mipCount), firstArraySlice(firstArraySlice), arraySize(arraySize) {}

		ResourceViewInfo(uint32_t firstElement, uint32_t elementCount)
			: firstElement(firstElement), elementCount(elementCount) {}

		static const uint32_t kMaxPossible = -1;

		// Textures
		uint32_t mostDetailedMip = 0;
		uint32_t mipCount = kMaxPossible;
		uint32_t firstArraySlice = 0;
		uint32_t arraySize = kMaxPossible;

		// Buffers
		uint32_t firstElement = 0;
		uint32_t elementCount = kMaxPossible;

		bool operator==(const ResourceViewInfo& other) const
		{
			return (firstArraySlice == other.firstArraySlice)
				&& (arraySize == other.arraySize)
				&& (mipCount == other.mipCount)
				&& (mostDetailedMip == other.mostDetailedMip)
				&& (firstElement == other.firstElement)
				&& (elementCount == other.elementCount);
		}
	};

	/** Abstracts API resource views.
	*/
	template<typename ApiHandleType>
	class ResourceView
	{
	public:
		using ApiHandle = ApiHandleType;
		static const uint32_t kMaxPossible = -1;
		virtual ~ResourceView();

		ResourceView(ResourceWeakPtr& pResource, ApiHandle handle, uint32_t mostDetailedMip, 
			uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
			: mApiHandle(handle), mpResource(pResource), mViewInfo(mostDetailedMip, mipCount, firstArraySlice, arraySize) {}

		ResourceView(ResourceWeakPtr& pResource, ApiHandle handle, uint32_t firstElement, uint32_t elementCount)
			: mApiHandle(handle), mpResource(pResource), mViewInfo(firstElement, elementCount) {}

		/** Get the raw API handle.
		*/
		const ApiHandle& getApiHandle() const { return mApiHandle; }

		/** Get information about the view.
		*/
		const ResourceViewInfo& getViewInfo() const { return mViewInfo; }

		/** Get the resource referenced by the view.
		*/
		Resource* getResource() const { return mpResource.lock().get(); }
	protected:
		ApiHandle mApiHandle;
		ResourceViewInfo mViewInfo;
		ResourceWeakPtr mpResource;
	};

	class ShaderResourceView : public ResourceView<SrvHandle>
	{
	public:
		using SharedPtr = std::shared_ptr<ShaderResourceView>;
		using SharedConstPtr = std::shared_ptr<const ShaderResourceView>;

		static SharedPtr create(ConstTextureSharedPtrRef pTexture, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize);
		static SharedPtr create(ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount);
		static SharedPtr getNullView();

		// This is currently used by RtScene to create an SRV for the TLAS, since the create() functions above assume texture or buffer types.
		ShaderResourceView(ResourceWeakPtr pResource, ApiHandle handle, uint32_t mostDetailedMip, uint32_t mipCount, uint32_t firstArraySlice, uint32_t arraySize)
			: ResourceView(pResource, handle, mostDetailedMip, mipCount, firstArraySlice, arraySize) {}
	private:

		ShaderResourceView(ResourceWeakPtr pResource, ApiHandle handle, uint32_t firstElement, uint32_t elementCount)
			: ResourceView(pResource, handle, firstElement, elementCount) {}
	};

	class DepthStencilView : public ResourceView<DsvHandle>
	{
	public:
		using SharedPtr = std::shared_ptr<DepthStencilView>;
		using SharedConstPtr = std::shared_ptr<const DepthStencilView>;

		static SharedPtr create(ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
		static SharedPtr getNullView();
	private:
		DepthStencilView(ResourceWeakPtr pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
			ResourceView(pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}
	};

	class UnorderedAccessView : public ResourceView<UavHandle>
	{
	public:
		using SharedPtr = std::shared_ptr<UnorderedAccessView>;
		using SharedConstPtr = std::shared_ptr<const UnorderedAccessView>;

		static SharedPtr create(ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
		static SharedPtr create(ConstBufferSharedPtrRef pBuffer, uint32_t firstElement, uint32_t elementCount);
		static SharedPtr getNullView();
	private:
		UnorderedAccessView(ResourceWeakPtr pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
			ResourceView(pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}

		UnorderedAccessView(ResourceWeakPtr pResource, ApiHandle handle, uint32_t firstElement, uint32_t elementCount)
			: ResourceView(pResource, handle, firstElement, elementCount) {}
	};

	class RenderTargetView : public ResourceView<RtvHandle>
	{
	public:
		using SharedPtr = std::shared_ptr<RenderTargetView>;
		using SharedConstPtr = std::shared_ptr<const RenderTargetView>;
		static SharedPtr create(ConstTextureSharedPtrRef pTexture, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize);
		static SharedPtr getNullView();
		~RenderTargetView();
	private:
		RenderTargetView(ResourceWeakPtr pResource, ApiHandle handle, uint32_t mipLevel, uint32_t firstArraySlice, uint32_t arraySize) :
			ResourceView(pResource, handle, mipLevel, 1, firstArraySlice, arraySize) {}
	};

	class ConstantBufferView : public ResourceView<CbvHandle>
	{
	public:
		using SharedPtr = std::shared_ptr<ConstantBufferView>;
		using SharedConstPtr = std::shared_ptr<const ConstantBufferView>;
		static SharedPtr create(ConstBufferSharedPtrRef pBuffer);
		static SharedPtr getNullView();

	private:
		ConstantBufferView(ResourceWeakPtr pResource, ApiHandle handle) :
			ResourceView(pResource, handle, 0, 1, 0, 1) {}
	};

	struct NullResourceViews
	{
		ShaderResourceView::SharedPtr srv;
		ConstantBufferView::SharedPtr cbv;
		RenderTargetView::SharedPtr rtv;
		UnorderedAccessView::SharedPtr uav;
		DepthStencilView::SharedPtr dsv;
	};
}