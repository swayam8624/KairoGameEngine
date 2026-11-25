#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace kairo::player::detail
{
    // Single producer (game thread), single consumer (audio callback).
    // No allocation, locks or exceptions on the consumer path. All counters
    // count stereo frames, never samples. Overflow drops newest frames;
    // underflow fills the missing output with silence.
    class RuntimePcmQueue final
    {
        static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
        std::vector<float> m_Data;
        const std::size_t m_Capacity;
        alignas(64) std::atomic<std::uint64_t> m_Write{0};
        alignas(64) std::atomic<std::uint64_t> m_Read{0};
        static std::size_t CheckedCapacity(std::size_t frames)
        {
            if(!frames||frames>262144||(frames&(frames-1)))
                throw std::invalid_argument("PCM capacity must be a bounded power of two.");
            return frames;
        }
    public:
        explicit RuntimePcmQueue(std::size_t capacity=16384)
            :m_Data(CheckedCapacity(capacity)*2),m_Capacity(capacity){}
        RuntimePcmQueue(const RuntimePcmQueue&)=delete;
        RuntimePcmQueue& operator=(const RuntimePcmQueue&)=delete;
        [[nodiscard]] std::size_t Capacity() const noexcept{return m_Capacity;}
        std::size_t Push(std::span<const float> stereo)
        {
            if(stereo.size()%2)throw std::invalid_argument("PCM must contain complete stereo frames.");
            const auto write=m_Write.load(std::memory_order_relaxed);
            const auto read=m_Read.load(std::memory_order_acquire);
            const auto frames=std::min(stereo.size()/2,m_Capacity-static_cast<std::size_t>(write-read));
            for(std::size_t i=0;i<frames;++i)
            {
                const auto offset=static_cast<std::size_t>((write+i)&(m_Capacity-1))*2;
                for(std::size_t channel=0;channel<2;++channel)
                {
                    const float value=stereo[2*i+channel];
                    m_Data[offset+channel]=std::isfinite(value)?std::clamp(value,-1.f,1.f):0.f;
                }
            }
            m_Write.store(write+frames,std::memory_order_release);
            return frames;
        }
        std::size_t Pop(std::span<float> stereo) noexcept
        {
            const auto read=m_Read.load(std::memory_order_relaxed);
            const auto write=m_Write.load(std::memory_order_acquire);
            const auto frames=std::min(stereo.size()/2,static_cast<std::size_t>(write-read));
            for(std::size_t i=0;i<frames;++i)
            {
                const auto offset=static_cast<std::size_t>((read+i)&(m_Capacity-1))*2;
                stereo[2*i]=m_Data[offset];stereo[2*i+1]=m_Data[offset+1];
            }
            std::fill(stereo.begin()+static_cast<std::ptrdiff_t>(frames*2),stereo.end(),0.f);
            m_Read.store(read+frames,std::memory_order_release);
            return frames;
        }
    };
}
