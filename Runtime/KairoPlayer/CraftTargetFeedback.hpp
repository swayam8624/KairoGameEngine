#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace kairo::player::detail
{
    using CraftTargetCell=std::array<std::int32_t,3>;
    struct CraftTargetKey
    {
        CraftTargetCell Cell{},Origin{};
        bool Mineable=false,Placeable=false;
        std::optional<CraftTargetCell> Placement;
        bool operator==(const CraftTargetKey&) const=default;
    };
    struct CraftTargetBox
    {
        std::array<float,3> Min{},Max{},Color{};
    };
    // Subtract signed world coordinates in int64 before converting to float.
    // A small expansion avoids coplanar depth fighting on visible block edges.
    inline CraftTargetBox CraftFeedbackBox(CraftTargetCell cell,CraftTargetCell origin,std::array<float,3> color)
    {
        CraftTargetBox box;box.Color=color;
        for(unsigned axis=0;axis<3;++axis)
        {
            const double p=double(std::int64_t(cell[axis])-origin[axis]);
            box.Min[axis]=static_cast<float>(p-.015);
            box.Max[axis]=static_cast<float>(p+1.015);
        }
        return box;
    }
    struct CraftTargetBoxes
    {
        CraftTargetBox Target;
        std::optional<CraftTargetBox> Placement;
    };
    inline CraftTargetBoxes CraftBuildTargetBoxes(const CraftTargetKey& key)
    {
        CraftTargetBoxes boxes{CraftFeedbackBox(key.Cell,key.Origin,key.Mineable?
            std::array<float,3>{1.f,.85f,.25f}:std::array<float,3>{1.f,.22f,.16f}),std::nullopt};
        if(key.Placement)boxes.Placement=CraftFeedbackBox(*key.Placement,key.Origin,key.Placeable?
            std::array<float,3>{.25f,.95f,.48f}:std::array<float,3>{1.f,.22f,.16f});
        return boxes;
    }
    class CraftTargetPublication
    {
        std::optional<CraftTargetKey> m_Previous;
        bool m_Initialized=false;
    public:
        template<class Submit> bool Refresh(std::optional<CraftTargetKey> key,Submit&& submit)
        {
            if(m_Initialized&&m_Previous==key)return false;
            submit(key); // failed renderer submission must be retryable
            m_Previous=key;m_Initialized=true;return true;
        }
    };
}
