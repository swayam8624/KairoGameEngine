#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace kairo::player::detail
{
    // A renderer-independent world-item store. Games supply item IDs, a
    // visibility policy, and a destination inventory transaction. Positions
    // stay in absolute doubles; only presentation converts to render space.
    template<std::size_t Capacity=128>
    class RuntimePickupPool
    {
    public:
        // Compile-time bounded capacity: no per-spawn allocation or eviction.
        static_assert(Capacity>0);
        using Position=std::array<double,3>;
        // Absolute world-space center, game-owned kind ID, remaining count,
        // and lifetime in seconds. No renderer or inventory pointer is stored.
        struct Item
        {
            Position Center;
            std::uint32_t Kind,Count;
            double Remaining;
        };
        using Storage=std::array<std::optional<Item>,Capacity>;
        // Borrow a read-only snapshot view until the next mutation; renderers
        // can rebuild presentation from these occupied slots.
        [[nodiscard]] const Storage& Items() const noexcept{return m_Items;}
        // Presentation generation changes on structural/count changes, not
        // countdown alone; compare for inequality rather than interpreting time.
        [[nodiscard]] std::uint64_t Revision() const noexcept{return m_Revision;}
        // Validate before touching storage. First free slot wins deterministically;
        // false means capacity exhausted and caller must choose reward policy.
        [[nodiscard]] bool TrySpawn(Position center,std::uint32_t kind,std::uint32_t count,double lifetime=120.)
        {
            Validate(center);
            if(!kind||!count||!std::isfinite(lifetime)||lifetime<=0)
                throw std::invalid_argument("Invalid pickup.");
            for(auto& slot:m_Items)if(!slot)
            {
                slot=Item{center,kind,count,lifetime};++m_Revision;return true;
            }
            return false;
        }
        // Expire slots at their lifetime boundary; elapsed is in seconds.
        // Invalid elapsed leaves every slot unchanged.
        void Advance(double elapsed)
        {
            if(!std::isfinite(elapsed)||elapsed<0)throw std::invalid_argument("Invalid pickup elapsed time.");
            for(auto& slot:m_Items)if(slot)
            {
                if(elapsed>=slot->Remaining){slot.reset();++m_Revision;}
                else slot->Remaining-=elapsed;
            }
        }
        // Credit must return the exact number actually accepted, at most
        // requested. Partial transfers keep the remainder in the same slot.
        // Each slot is processed once, so revisiting cannot duplicate items.
        // Visible is a game-provided ray/occlusion predicate. Credit is the
        // inventory transaction: it returns only accepted items. A throwing
        // callback can leave earlier slots processed, so this is per-item atomic.
        template<class Visible,class Credit>
        void Collect(Position origin,double radius,Visible&& visible,Credit&& credit)
        {
            Validate(origin);
            if(!std::isfinite(radius)||radius<0||radius>64)
                throw std::invalid_argument("Invalid pickup radius.");
            for(auto& slot:m_Items)if(slot)
            {
                const auto& p=slot->Center;
                if(std::hypot(p[0]-origin[0],p[1]-origin[1],p[2]-origin[2])>radius||
                    !visible(origin,p))continue;
                const auto accepted=credit(slot->Kind,slot->Count);
                if(accepted>slot->Count)throw std::logic_error("Pickup inventory over-credit.");
                if(!accepted)continue;
                slot->Count-=accepted;
                if(!slot->Count)slot.reset();
                ++m_Revision;
            }
        }
        // Remove all items without touching external inventories.
        void Clear() noexcept
        {
            for(auto& slot:m_Items)if(slot){slot.reset();++m_Revision;}
        }
    private:
        // Reject NaN/Inf and huge coordinates before distance calculation.
        static void Validate(const Position& p)
        {
            for(const auto axis:p)if(!std::isfinite(axis)||std::abs(axis)>1.1e9)
                throw std::invalid_argument("Invalid pickup position.");
        }
        Storage m_Items{};
        std::uint64_t m_Revision=0;
    };
}
