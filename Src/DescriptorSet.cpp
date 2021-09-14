#include "DescriptorSet.h"

namespace WIP3D
{
    DescriptorSet::SharedPtr DescriptorSet::create(const DescriptorPool::SharedPtr& pPool, const Layout& layout)
    {
        return SharedPtr(new DescriptorSet(pPool, layout));
    }

    DescriptorSet::DescriptorSet(DescriptorPool::SharedPtr pPool, const Layout& layout)
        : mpPool(pPool)
        , mLayout(layout)
    {
        apiInit();
    }

    DescriptorSet::~DescriptorSet()
    {
        mpPool->releaseAllocation(mpApiData);
    }

    DescriptorSet::Layout& DescriptorSet::Layout::addRange(DescriptorSet::Type type, uint32_t baseRegIndex, uint32_t descriptorCount, uint32_t regSpace)
    {
        Range r;
        r.descCount = descriptorCount;
        r.baseRegIndex = baseRegIndex;
        r.regSpace = regSpace;
        r.type = type;

        mRanges.push_back(r);
        return *this;
    }
}
