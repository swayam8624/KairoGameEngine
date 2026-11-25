#pragma once
#include "RuntimePcmQueue.hpp"
#include <string>
#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace kairo::player::detail
{
    // Optional platform output. The game thread is the sole PCM producer.
    // Audio Queue owns three fixed-size buffers; its callback consumes the
    // ring without allocation or scene access. Dispose synchronously before
    // destroying the ring. Non-Apple hosts retain the headless mixer.
    class RuntimeNativeAudioOutput final
    {
#if defined(__APPLE__)
        static constexpr UInt32 FramesPerBuffer=512;
        static constexpr UInt32 BufferBytes=FramesPerBuffer*2*sizeof(float);
        RuntimePcmQueue m_Pcm;
        AudioQueueRef m_Queue=nullptr;
        std::atomic<bool> m_Stopping{false};
        std::atomic<OSStatus> m_Error{noErr};
        static_assert(std::atomic<OSStatus>::is_always_lock_free);
        static_assert(std::atomic<bool>::is_always_lock_free);

        static void Check(OSStatus status,const char* operation)
        {
            if(status!=noErr)throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(status));
        }
        static void Callback(void* user,AudioQueueRef queue,AudioQueueBufferRef buffer) noexcept
        {
            auto& output=*static_cast<RuntimeNativeAudioOutput*>(user);
            if(output.m_Stopping.load(std::memory_order_acquire))return;
            if(!buffer||!buffer->mAudioData||buffer->mAudioDataBytesCapacity<BufferBytes)
            {
                output.m_Error.store(-1,std::memory_order_relaxed);return;
            }
            (void)output.m_Pcm.Pop(std::span(static_cast<float*>(buffer->mAudioData),FramesPerBuffer*2));
            buffer->mAudioDataByteSize=BufferBytes;
            if(output.m_Stopping.load(std::memory_order_acquire))return;
            const auto status=AudioQueueEnqueueBuffer(queue,buffer,0,nullptr);
            if(status!=noErr)output.m_Error.store(status,std::memory_order_relaxed);
        }
        void Close() noexcept
        {
            m_Stopping.store(true,std::memory_order_release);
            if(m_Queue)
            {
                (void)AudioQueueStop(m_Queue,true);
                (void)AudioQueueDispose(m_Queue,true);
                m_Queue=nullptr;
            }
        }
#endif
    public:
        explicit RuntimeNativeAudioOutput(std::uint32_t rate)
        {
            if(rate<8000||rate>192000)throw std::invalid_argument("Unsupported native audio sample rate.");
#if defined(__APPLE__)
            AudioStreamBasicDescription format{};
            format.mSampleRate=rate;
            format.mFormatID=kAudioFormatLinearPCM;
            format.mFormatFlags=kAudioFormatFlagIsFloat|kAudioFormatFlagIsPacked|kAudioFormatFlagsNativeEndian;
            format.mBytesPerPacket=2*sizeof(float);
            format.mFramesPerPacket=1;
            format.mBytesPerFrame=2*sizeof(float);
            format.mChannelsPerFrame=2;
            format.mBitsPerChannel=32;
            try
            {
                Check(AudioQueueNewOutput(&format,Callback,this,nullptr,nullptr,0,&m_Queue),"AudioQueueNewOutput");
                for(unsigned i=0;i<3;++i)
                {
                    AudioQueueBufferRef buffer=nullptr;
                    Check(AudioQueueAllocateBuffer(m_Queue,BufferBytes,&buffer),"AudioQueueAllocateBuffer");
                    std::fill_n(static_cast<float*>(buffer->mAudioData),FramesPerBuffer*2,0.f);
                    buffer->mAudioDataByteSize=BufferBytes;
                    Check(AudioQueueEnqueueBuffer(m_Queue,buffer,0,nullptr),"AudioQueueEnqueueBuffer");
                }
                Check(AudioQueueStart(m_Queue,nullptr),"AudioQueueStart");
            }
            catch(...){Close();throw;}
#endif
        }
        ~RuntimeNativeAudioOutput()
        {
#if defined(__APPLE__)
            Close();
#endif
        }
        RuntimeNativeAudioOutput(const RuntimeNativeAudioOutput&)=delete;
        RuntimeNativeAudioOutput& operator=(const RuntimeNativeAudioOutput&)=delete;
        [[nodiscard]] bool Enabled() const noexcept
        {
#if defined(__APPLE__)
            return m_Queue!=nullptr;
#else
            return false;
#endif
        }
        [[nodiscard]] bool Failed() const noexcept
        {
#if defined(__APPLE__)
            return m_Error.load(std::memory_order_relaxed)!=noErr;
#else
            return false;
#endif
        }
        std::size_t Push(std::span<const float> pcm)
        {
#if defined(__APPLE__)
            return m_Pcm.Push(pcm);
#else
            (void)pcm;return 0;
#endif
        }
    };
}
