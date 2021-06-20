#pragma once
#include <memory>
#include <unordered_map>
#include "Formats.h"
#include "GraphicsResView.h"
#include "Vector3.h"
#include "Common/Logger.h"
#include "Vector4.h"
#include "Vector2.h"
#include "GPUMemory.h"
#include "GraphicsResource.h"

namespace WIP3D
{
    // Resources
    Resource::~Resource() = default;

    const std::string to_string(Resource::Type type)
    {
#define type_2_string(a) case Resource::Type::a: return #a;
        switch (type)
        {
            type_2_string(Buffer);
            type_2_string(Texture1D);
            type_2_string(Texture2D);
            type_2_string(Texture3D);
            type_2_string(TextureCube);
            type_2_string(Texture2DMultisample);
        default:
            should_not_get_here();
            return "";
        }
#undef type_2_string
    }

    const std::string to_string(Resource::State state)
    {
        if (state == Resource::State::Common)
        {
            return "Common";
        }
        std::string s;
#define state_to_str(f_) if (state == Resource::State::f_) {return #f_; }

        state_to_str(Common);
        state_to_str(VertexBuffer);
        state_to_str(ConstantBuffer);
        state_to_str(IndexBuffer);
        state_to_str(RenderTarget);
        state_to_str(UnorderedAccess);
        state_to_str(DepthStencil);
        state_to_str(ShaderResource);
        state_to_str(StreamOut);
        state_to_str(IndirectArg);
        state_to_str(CopyDest);
        state_to_str(CopySource);
        state_to_str(ResolveDest);
        state_to_str(ResolveSource);
        state_to_str(Present);
        state_to_str(Predication);
        state_to_str(NonPixelShader);
#ifdef WIP_D3D12
        state_to_str(AccelerationStructure);
#endif
#undef state_to_str
        return s;
    }

    void Resource::invalidateViews() const
    {
        mSrvs.clear();
        mUavs.clear();
        mRtvs.clear();
        mDsvs.clear();
    }

    Resource::State Resource::getGlobalState() const
    {
        if (mState.isGlobal == false)
        {
            LOG_WARN("Resource::getGlobalState() - the resource doesn't have a global state. The subresoruces are in a different state, use getSubResourceState() instead");
            return State::Undefined;
        }
        return mState.global;
    }

    Resource::State Resource::getSubresourceState(uint32_t arraySlice, uint32_t mipLevel) const
    {
        const Texture* pTexture = dynamic_cast<const Texture*>(this);
        if (pTexture)
        {
            uint32_t subResource = pTexture->getSubresourceIndex(arraySlice, mipLevel);
            return (mState.isGlobal) ? mState.global : mState.perSubresource[subResource];
        }
        else
        {
            LOG_WARN("Calling Resource::getSubresourceState() on an object that is not a texture. This call is invalid, use Resource::getGlobalState() instead");
            assert(mState.isGlobal);
            return mState.global;
        }
    }

    void Resource::setGlobalState(State newState) const
    {
        mState.isGlobal = true;
        mState.global = newState;
    }

    void Resource::setSubresourceState(uint32_t arraySlice, uint32_t mipLevel, State newState) const
    {
        const Texture* pTexture = dynamic_cast<const Texture*>(this);
        if (pTexture == nullptr)
        {
            LOG_WARN("Calling Resource::setSubresourceState() on an object that is not a texture. This is invalid. Ignoring call");
            return;
        }

        // If we are transitioning from a global to local state, initialize the subresource array
        if (mState.isGlobal)
        {
            std::fill(mState.perSubresource.begin(), mState.perSubresource.end(), mState.global);
        }
        mState.isGlobal = false;
        mState.perSubresource[pTexture->getSubresourceIndex(arraySlice, mipLevel)] = newState;
    }

}
