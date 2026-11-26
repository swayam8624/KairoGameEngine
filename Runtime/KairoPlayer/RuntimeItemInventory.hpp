#pragma once
#include <cstdint>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace kairo::player::detail
{
    // Stable authored item IDs are independent of renderer handles and voxel
    // materials. The catalog defines stack limits; the counts are live state.
    // CraftInventory remains the specialized voxel-block inventory.
    class RuntimeItemInventory
    {
    public:
        using Counts=std::map<std::string,unsigned,std::less<>>;
        struct Change{std::string_view ID;int Delta;};
        explicit RuntimeItemInventory(Counts limits):m_Limits(std::move(limits)),m_Counts(m_Limits)
        {
            if(m_Limits.empty()||m_Limits.size()>256)throw std::invalid_argument("Invalid item catalog size.");
            for(const auto& [id,limit]:m_Limits)
            {
                if(id.empty()||id.size()>64||!limit||limit>999)throw std::invalid_argument("Invalid item definition.");
                for(unsigned char c:id)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'))
                    throw std::invalid_argument("Invalid item ID.");
                m_Counts.at(id)=0;
            }
        }
        [[nodiscard]] unsigned Count(std::string_view id) const
        {
            const auto it=m_Counts.find(id);
            if(it==m_Counts.end())throw std::invalid_argument("Unknown item ID.");
            return it->second;
        }
        [[nodiscard]] const Counts& Snapshot() const noexcept{return m_Counts;}
        // Aggregate all changes before publishing. A trade cannot depend on
        // transient ordering or consume a cost if another reward overflows.
        // Limit the number of deltas, and use wide arithmetic for hostile ints.
        bool Apply(std::span<const Change> changes)
        {
            if(changes.size()>256)throw std::invalid_argument("Too many item changes.");
            std::map<std::string_view,std::int64_t> net;
            for(const auto& change:changes){(void)Count(change.ID);net[change.ID]+=change.Delta;}
            auto candidate=m_Counts;
            for(const auto& [id,delta]:net)
            {
                auto it=candidate.find(id);
                const auto next=static_cast<std::int64_t>(it->second)+delta;
                if(next<0||next>m_Limits.find(id)->second)return false;
                it->second=static_cast<unsigned>(next);
            }
            m_Counts.swap(candidate);return true;
        }
        // Validate the complete snapshot against the same immutable catalog.
        // Take by value so a failed validation cannot mutate the live counts.
        void Restore(Counts counts)
        {
            if(counts.size()!=m_Limits.size())throw std::invalid_argument("Item snapshot size mismatch.");
            for(const auto& [id,count]:counts)
            {
                const auto it=m_Limits.find(id);
                if(it==m_Limits.end()||count>it->second)throw std::invalid_argument("Invalid item snapshot.");
            }
            m_Counts.swap(counts);
        }
    private:
        Counts m_Limits,m_Counts;
    };
}
