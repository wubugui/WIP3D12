#pragma once
#include "./D3D12/WIPD3D12.h"
#include "DescriptorSet.h"
#include "RenderTarget.h"
#include "Shader.h"

namespace WIP3D
{
    class ProgramReflection;
    class EntryPointGroupReflection;
    class CopyContext;

    /** The root signature defines what resources are bound to the pipeline.

        The layout is defined by traversing the ParameterBlock hierarchy
        of a program to find all required root parameters. These are then
        arranged consecutively in the following order in the root signature:

        1. descriptor tables
        2. root descriptors
        3. root constants

        The get*BaseIndex() functions return the base index of the
        corresponding root parameter type in the root signature.
    */
    class RootSignature
    {
    public:
        using SharedPtr = std::shared_ptr<RootSignature>;
        using SharedConstPtr = std::shared_ptr<const RootSignature>;
        using ApiHandle = RootSignatureHandle;

        using DescType = WIP3D::DescriptorSet::Type;
        using DescriptorSetLayout = DescriptorSet::Layout;

        struct RootDescriptorDesc
        {
            DescType type;
            uint32_t regIndex;
            uint32_t spaceIndex;
            ShaderVisibility visibility;
        };

        struct RootConstantsDesc
        {
            uint32_t regIndex;
            uint32_t spaceIndex;
            uint32_t count;
        };

        class Desc
        {
        public:
            Desc& addDescriptorSet(const DescriptorSetLayout& setLayout);
            Desc& addRootDescriptor(DescType type, uint32_t regIndex, uint32_t spaceIndex, ShaderVisibility visibility = ShaderVisibility::All);
            Desc& addRootConstants(uint32_t regIndex, uint32_t spaceIndex, uint32_t count); // #SHADER_VAR Make sure this works with the reflectors

#ifdef FALCOR_D3D12
            Desc& setLocal(bool isLocal) { mIsLocal = isLocal; return *this; }
#endif

            size_t getSetsCount() const { return mSets.size(); }
            const DescriptorSetLayout& getSet(size_t index) const { return mSets[index]; }

            size_t getRootDescriptorCount() const { return mRootDescriptors.size(); }
            const RootDescriptorDesc& getRootDescriptorDesc(size_t index) const { return mRootDescriptors[index]; }

            size_t getRootConstantCount() const { return mRootConstants.size(); }
            const RootConstantsDesc& getRootConstantDesc(size_t index) const { return mRootConstants[index]; }

        private:
            friend class RootSignature;

            std::vector<DescriptorSetLayout> mSets;
            std::vector<RootDescriptorDesc> mRootDescriptors;
            std::vector<RootConstantsDesc> mRootConstants;

#ifdef FALCOR_D3D12
            bool mIsLocal = false;
#endif
        };

        ~RootSignature();

        /** Get an empty root signature.
            \return Empty root signature, or throws an exception on error.
        */
        static SharedPtr getEmpty();

        /** Create a root signature.
            \param[in] desc Root signature description.
            \return New object, or throws an exception if creation failed.
        */
        static SharedPtr create(const Desc& desc);

        /** Create a root signature from program reflection.
            \param[in] pReflection Reflection object.
            \return New object, or throws an exception if creation failed.
        */
        static SharedPtr create(const ProgramReflection* pReflection);

        /** Create a local root signature for use with DXR.
            \param[in] pReflection Reflection object.
            \return New object, or throws an exception if creation failed.
        */
        static SharedPtr createLocal(const EntryPointGroupReflection* pReflector);

        const ApiHandle& getApiHandle() const { return mApiHandle; }

        size_t getDescriptorSetCount() const { return mDesc.mSets.size(); }
        const DescriptorSetLayout& getDescriptorSet(size_t index) const { return mDesc.mSets[index]; }

        uint32_t getDescriptorSetBaseIndex() const { return 0; }
        uint32_t getRootDescriptorBaseIndex() const { return getDescriptorSetBaseIndex() + (uint32_t)mDesc.mSets.size(); }
        uint32_t getRootConstantBaseIndex() const { return getRootDescriptorBaseIndex() + (uint32_t)mDesc.mRootDescriptors.size(); }

        uint32_t getSizeInBytes() const { return mSizeInBytes; }
        uint32_t getElementByteOffset(uint32_t elementIndex) { return mElementByteOffset[elementIndex]; }

        void bindForGraphics(CopyContext* pCtx);
        void bindForCompute(CopyContext* pCtx);

        const Desc& getDesc() const { return mDesc; }

    protected:
        RootSignature(const Desc& desc);
        void apiInit();
#ifdef FALCOR_D3D12
        virtual void createApiHandle(ID3DBlobPtr pSigBlob);
#endif
        ApiHandle mApiHandle;
        Desc mDesc;
        static SharedPtr spEmptySig;
        static uint64_t sObjCount;

        uint32_t mSizeInBytes;
        std::vector<uint32_t> mElementByteOffset;
    };

    class ComputeStateObject
    {
    public:
        using SharedPtr = std::shared_ptr<ComputeStateObject>;
        using SharedConstPtr = std::shared_ptr<const ComputeStateObject>;
        using ApiHandle = ComputeStateHandle;

        class Desc
        {
        public:
            Desc& setRootSignature(RootSignature::SharedPtr pSignature) { mpRootSignature = pSignature; return *this; }
            Desc& setProgramKernels(const ProgramKernels::SharedConstPtr& pProgram) { mpProgram = pProgram; return *this; }
            const ProgramKernels::SharedConstPtr getProgramKernels() const { return mpProgram; }
            ProgramVersion::SharedConstPtr getProgramVersion() const { return mpProgram->getProgramVersion(); }
            bool operator==(const Desc& other) const;
        private:
            friend class ComputeStateObject;
            ProgramKernels::SharedConstPtr mpProgram;
            RootSignature::SharedPtr mpRootSignature;
        };

        ~ComputeStateObject();

        /** Create a compute state object.
            \param[in] desc State object description.
            \return New object, or throws an exception if creation failed.
        */
        static SharedPtr create(const Desc& desc);

        const ApiHandle& getApiHandle() { return mApiHandle; }
        const Desc& getDesc() const { return mDesc; }

    private:
        ComputeStateObject(const Desc& desc);
        void apiInit();

        Desc mDesc;
        ApiHandle mApiHandle;
    };

    class GraphicsStateObject
    {
    public:
        using SharedPtr = std::shared_ptr<GraphicsStateObject>;
        using SharedConstPtr = std::shared_ptr<const GraphicsStateObject>;
        using ApiHandle = GraphicsStateHandle;

        static const uint32_t kSampleMaskAll = -1;

        /** Primitive topology
        */
        enum class PrimitiveType
        {
            Undefined,
            Point,
            Line,
            Triangle,
            Patch,
        };

        class Desc
        {
        public:
            Desc& setRootSignature(RootSignature::SharedPtr pSignature) { mpRootSignature = pSignature; return *this; }
            Desc& setVertexLayout(VertexLayout::SharedConstPtr pLayout) { mpLayout = pLayout; return *this; }
            Desc& setFboFormats(const Fbo::Desc& fboFormats) { mFboDesc = fboFormats; return *this; }
            Desc& setProgramKernels(ProgramKernels::SharedConstPtr pProgram) { mpProgram = pProgram; return *this; }
            Desc& setBlendState(BlendState::SharedPtr pBlendState) { mpBlendState = pBlendState; return *this; }
            Desc& setRasterizerState(RasterizerState::SharedPtr pRasterizerState) { mpRasterizerState = pRasterizerState; return *this; }
            Desc& setDepthStencilState(DepthStencilState::SharedPtr pDepthStencilState) { mpDepthStencilState = pDepthStencilState; return *this; }
            Desc& setSampleMask(uint32_t sampleMask) { mSampleMask = sampleMask; return *this; }
            Desc& setPrimitiveType(PrimitiveType type) { mPrimType = type; return *this; }

            BlendState::SharedPtr getBlendState() const { return mpBlendState; }
            RasterizerState::SharedPtr getRasterizerState() const { return mpRasterizerState; }
            DepthStencilState::SharedPtr getDepthStencilState() const { return mpDepthStencilState; }
            ProgramKernels::SharedConstPtr getProgramKernels() const { return mpProgram; }
            ProgramVersion::SharedConstPtr getProgramVersion() const { return mpProgram->getProgramVersion(); }
            RootSignature::SharedPtr getRootSignature() const { return mpRootSignature; }
            uint32_t getSampleMask() const { return mSampleMask; }
            VertexLayout::SharedConstPtr getVertexLayout() const { return mpLayout; }
            PrimitiveType getPrimitiveType() const { return mPrimType; }
            Fbo::Desc getFboDesc() const { return mFboDesc; }

            bool operator==(const Desc& other) const;

        private:
            friend class GraphicsStateObject;
            Fbo::Desc mFboDesc;
            VertexLayout::SharedConstPtr mpLayout;
            ProgramKernels::SharedConstPtr mpProgram;
            RasterizerState::SharedPtr mpRasterizerState;
            DepthStencilState::SharedPtr mpDepthStencilState;
            BlendState::SharedPtr mpBlendState;
            uint32_t mSampleMask = kSampleMaskAll;
            RootSignature::SharedPtr mpRootSignature;
            PrimitiveType mPrimType = PrimitiveType::Undefined;
        };

        ~GraphicsStateObject();

        /** Create a graphics state object.
            \param[in] desc State object description.
            \return New object, or throws an exception if creation failed.
        */
        static SharedPtr create(const Desc& desc);

        const ApiHandle& getApiHandle() { return mApiHandle; }

        const Desc& getDesc() const { return mDesc; }

    private:
        GraphicsStateObject(const Desc& desc);
        void apiInit();

        Desc mDesc;
        ApiHandle mApiHandle;

        // Default state objects
        static BlendState::SharedPtr spDefaultBlendState;
        static RasterizerState::SharedPtr spDefaultRasterizerState;
        static DepthStencilState::SharedPtr spDefaultDepthStencilState;
    };
}
