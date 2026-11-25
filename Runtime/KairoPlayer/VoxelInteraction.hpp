#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace kairo::player::detail
{
    // Global signed voxel coordinates. Air is material zero; the other
    // values are game-defined material IDs. Keep generation immutable and
    // store only deviations from the baseline (including removed blocks).
    struct VoxelCoordinate
    {
        std::int32_t X=0,Y=0,Z=0;
        auto operator<=>(const VoxelCoordinate&) const=default;
    };
    using VoxelMaterial=std::uint16_t;
    inline constexpr VoxelMaterial VoxelAir=0;

    template<class Baseline>
    class VoxelEdits
    {
    public:
        explicit VoxelEdits(Baseline baseline,std::size_t capacity=65536)
            :m_Baseline(std::move(baseline)),m_Capacity(capacity) {}

        [[nodiscard]] VoxelMaterial Get(VoxelCoordinate cell) const
        {
            const auto it=m_Edits.find(cell);
            return it==m_Edits.end()?m_Baseline(cell):it->second;
        }
        // False means the edit cap rejected a new override. Reverting to
        // the baseline always frees a slot, even if the journal is full.
        [[nodiscard]] bool Set(VoxelCoordinate cell,VoxelMaterial value)
        {
            if(value==m_Baseline(cell)){m_Edits.erase(cell);return true;}
            if(auto it=m_Edits.find(cell);it!=m_Edits.end())
            {it->second=value;return true;}
            if(m_Edits.size()>=m_Capacity)return false;
            m_Edits.emplace(cell,value);
            return true;
        }
        [[nodiscard]] const std::map<VoxelCoordinate,VoxelMaterial>& Overrides() const noexcept
        {return m_Edits;}
    private:
        Baseline m_Baseline;
        std::size_t m_Capacity;
        std::map<VoxelCoordinate,VoxelMaterial> m_Edits;
    };

    struct VoxelHit
    {
        VoxelCoordinate Cell;
        // For a single-face entry this is the preceding face-adjacent
        // cell. For an exact corner/edge crossing or starting inside a
        // solid there is no unique adjacent face, so placement is omitted.
        std::optional<VoxelCoordinate> Placement;
        double Distance=0;
        VoxelMaterial Material=VoxelAir;
    };

    // Amanatides-Woo traversal of half-open unit cells. Simultaneous
    // boundary crossings advance all tied axes, skipping zero-length
    // edge/corner contacts. The callback owns availability policy:
    // nullopt is unknown/unloaded and terminates the query, never air.
    template<class Query>
    [[nodiscard]] std::optional<VoxelHit> TraceVoxels(
        std::array<double,3> origin,std::array<double,3> direction,
        double maximumDistance,std::size_t maximumCells,Query&& query)
    {
        if(!std::isfinite(maximumDistance)||maximumDistance<0||maximumDistance>1e6||
            maximumCells==0||maximumCells>1000000)
            throw std::invalid_argument("Invalid voxel ray budget.");
        const double norm=std::hypot(direction[0],direction[1],direction[2]);
        if(!std::isfinite(norm)||norm<=1e-12)
            throw std::invalid_argument("Invalid voxel ray direction.");
        std::array<std::int64_t,3> cell{};
        std::array<int,3> step{};
        std::array<double,3> next{},delta{};
        constexpr auto low=std::numeric_limits<std::int32_t>::min();
        constexpr auto high=std::numeric_limits<std::int32_t>::max();
        constexpr auto infinity=std::numeric_limits<double>::infinity();
        for(unsigned axis=0;axis<3;++axis)
        {
            if(!std::isfinite(origin[axis]))throw std::invalid_argument("Invalid voxel ray origin.");
            const double base=std::floor(origin[axis]);
            if(base<low||base>high)return {};
            cell[axis]=static_cast<std::int64_t>(base);
            direction[axis]/=norm;
            step[axis]=(direction[axis]>0)-(direction[axis]<0);
            // At an integer boundary moving negatively, the first
            // positive-length interval is in the cell on the left.
            if(step[axis]<0&&origin[axis]==base)--cell[axis];
            if(cell[axis]<low||cell[axis]>high)return {};
            if(step[axis]==0){next[axis]=delta[axis]=infinity;continue;}
            const double boundary=static_cast<double>(cell[axis]+(step[axis]>0?1:0));
            next[axis]=(boundary-origin[axis])/direction[axis];
            delta[axis]=1/std::abs(direction[axis]);
        }
        const auto coordinate=[&]{return VoxelCoordinate{static_cast<std::int32_t>(cell[0]),
            static_cast<std::int32_t>(cell[1]),static_cast<std::int32_t>(cell[2])};};
        double distance=0;
        std::optional<VoxelCoordinate> placement;
        for(std::size_t visited=0;visited<maximumCells;++visited)
        {
            const auto current=coordinate();
            const std::optional<VoxelMaterial> material=query(current);
            if(!material)return {};
            if(*material!=VoxelAir)return VoxelHit{current,placement,distance,*material};
            const double crossing=std::min({next[0],next[1],next[2]});
            if(!std::isfinite(crossing)||crossing>maximumDistance)return {};
            unsigned axes=0;
            for(unsigned axis=0;axis<3;++axis)
                if(next[axis]==crossing)
                {
                    ++axes;
                    cell[axis]+=step[axis];
                    if(cell[axis]<low||cell[axis]>high)return {};
                    next[axis]+=delta[axis];
                }
            placement=axes==1?std::optional<VoxelCoordinate>{current}:std::nullopt;
            distance=crossing;
        }
        return {};
    }
}
