#pragma once
#include "RuntimePcmQueue.hpp"
#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>

namespace kairo::player::detail
{
    // A nonblocking device may accept only part of a period. Preserve the
    // unsent frames (including silence) until the device accepts them.
    // The consumer owns this object; the game thread only pushes the queue.
    template<std::size_t PeriodFrames=512>
    class RuntimePcmPump final
    {
        // Fixed device-period storage prevents allocation in the consumer loop.
        static_assert(PeriodFrames>0&&PeriodFrames<=8192);
        // Borrow the queue; it must outlive this pump. Offset counts frames,
        // not floats, and starts at end to trigger a first refill.
        RuntimePcmQueue& m_Queue;
        std::array<float,PeriodFrames*2> m_Period{};
        std::size_t m_Offset=PeriodFrames;
    public:
        // Construct on the device/consumer side after queue allocation.
        explicit RuntimePcmPump(RuntimePcmQueue& queue) noexcept:m_Queue(queue){}
        // Write is a device-specific callback accepting interleaved floats and
        // returning accepted stereo frames (possibly zero). Refill only after
        // the previous period is exhausted; partial writes and silence persist.
        // Reject impossible over-credit without advancing the offset.
        template<class Write>
        std::size_t Step(Write&& write)
        {
            if(m_Offset==PeriodFrames)
            {
                (void)m_Queue.Pop(m_Period);
                m_Offset=0;
            }
            // Convert frame offset to sample offset when taking the span.
            // The view is valid only for this call; device must not retain it.
            const auto pending=std::span<const float>(m_Period).subspan(m_Offset*2);
            const std::size_t accepted=write(pending);
            if(accepted>pending.size()/2)throw std::logic_error("Audio device accepted more frames than offered.");
            m_Offset+=accepted;
            return accepted;
        }
    };
}
