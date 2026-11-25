#pragma once
#include <array>
#include <optional>
#include <type_traits>
#include <cstddef>
#include <stdexcept>

namespace kairo::player::detail
{
    // Bounded multi-chunk publication. Handle creation must transfer
    // ownership only on success. Submit may throw after partially changing
    // the renderer: on that path keep BOTH generations until a successful
    // restoration proves the staged handles are no longer referenced.
    template<class Handle,std::size_t N>
    class VoxelMeshPublication final
    {
        static_assert(std::is_nothrow_copy_constructible_v<Handle>);
        static_assert(std::is_nothrow_copy_assignable_v<Handle>);
    public:
        using Slots=std::array<std::optional<Handle>,N>;
        [[nodiscard]] const Slots& Active() const noexcept {return m_Active;}
        [[nodiscard]] bool NeedsRestore() const noexcept {return m_Restore;}
        [[nodiscard]] std::size_t RetiredCount() const noexcept
        {
            std::size_t n=0;
            for(const auto& h:m_Retired)n+=h.has_value();
            return n;
        }

        template<class Submit,class Destroy>
        bool Recover(Submit&& submit,Destroy&& destroy)
        {
            if(m_Restore){submit(m_Active);m_Restore=false;}
            Drain(destroy);
            return RetiredCount()==0;
        }

        // Stage at most a caller-specified number of native uploads per
        // update, without changing the submitted scene or active handles.
        [[nodiscard]] bool Staging() const noexcept {return m_Staging;}
        template<class Submit,class Destroy>
        bool BeginStaging(const std::array<bool,N>& dirty,Submit&& submit,Destroy&& destroy)
        {
            if(m_Staging)throw std::logic_error("Voxel staging already active.");
            if(!Recover(submit,destroy))return false;
            m_Dirty=dirty;m_Cursor=0;m_Staging=true;
            return true;
        }
        template<class Create>
        bool Stage(std::size_t budget,Create&& create)
        {
            if(!m_Staging||budget==0)throw std::invalid_argument("Invalid voxel staging request.");
            for(;m_Cursor<N;++m_Cursor)
            {
                if(!m_Dirty[m_Cursor])continue;
                if(budget==0)return false;
                m_Staged[m_Cursor]=create(m_Cursor);
                --budget;
            }
            return true;
        }
        template<class Destroy>
        void AbortStaging(Destroy&& destroy) noexcept
        {
            if(!m_Staging)return;
            // BeginStaging required an empty retirement journal. No
            // staged handle was submitted, so these can retire immediately.
            m_Retired=m_Staged;m_Staged={};m_Staging=false;
            Drain(destroy);
        }
        template<class Submit,class Destroy>
        void CommitStaging(Submit&& submit,Destroy&& destroy)
        {
            if(!m_Staging||m_Cursor!=N)throw std::logic_error("Voxel staging incomplete.");
            auto next=m_Active;
            for(std::size_t i=0;i<N;++i)if(m_Dirty[i])next[i]=m_Staged[i];
            try{submit(next);}
            catch(...)
            {
                m_Retired=m_Staged;m_Staged={};m_Staging=false;m_Restore=true;
                throw;
            }
            for(std::size_t i=0;i<N;++i)if(m_Dirty[i])m_Retired[i]=m_Active[i];
            m_Active=next;m_Staged={};m_Staging=false;
            Drain(destroy);
        }

        // True means the new scene was published, even if a retirement
        // failed. False means the old scene remains and no upload started.
        template<class Create,class Submit,class Destroy>
        bool Apply(const std::array<bool,N>& dirty,Create&& create,
            Submit&& submit,Destroy&& destroy)
        {
            if(m_Staging)throw std::logic_error("Cannot publish an edit during staging.");
            if(!Recover(submit,destroy))return false;
            Slots staged{};
            auto next=m_Active;
            try
            {
                for(std::size_t i=0;i<N;++i)if(dirty[i])
                {
                    staged[i]=create(i); // empty mesh is an empty slot
                    next[i]=staged[i];
                }
            }
            catch(...)
            {
                m_Retired=staged;
                Drain(destroy);
                throw;
            }
            try{submit(next);}
            catch(...)
            {
                m_Retired=staged;
                m_Restore=true;
                throw;
            }
            // No allocation or throwing operation between successful
            // scene submission and ownership publication.
            for(std::size_t i=0;i<N;++i)if(dirty[i])m_Retired[i]=m_Active[i];
            m_Active=next;
            Drain(destroy);
            return true;
        }

        // Terminal cleanup, retryable. Empty-scene submission must succeed
        // before either active or potentially referenced retired handles
        // are destroyed.
        template<class Submit,class Destroy>
        std::size_t Release(Submit&& submit,Destroy&& destroy) noexcept
        {
            AbortStaging(destroy);
            try{submit(Slots{});}catch(...){return CountActive()+RetiredCount();}
            m_Restore=false;
            Drain(destroy);
            for(auto& h:m_Active)if(h)
            {
                try{destroy(*h);h.reset();}catch(...){}
            }
            return CountActive()+RetiredCount();
        }
    private:
        template<class Destroy> void Drain(Destroy&& destroy) noexcept
        {
            for(auto& h:m_Retired)if(h)
            {
                try{destroy(*h);h.reset();}catch(...){}
            }
        }
        std::size_t CountActive() const noexcept
        {
            std::size_t n=0;
            for(const auto& h:m_Active)n+=h.has_value();
            return n;
        }
        Slots m_Active{},m_Retired{},m_Staged{};
        std::array<bool,N> m_Dirty{};
        std::size_t m_Cursor=0;
        bool m_Staging=false;
        bool m_Restore=false;
    };
}
