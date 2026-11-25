#pragma once
#include "RuntimePcmPump.hpp"
#include <alsa/asoundlib.h>
#include <cerrno>
#include <chrono>
#include <string>
#include <thread>

namespace kairo::player::detail
{
    // Linux output owns exactly one nonblocking PCM stream. The consumer
    // worker never touches the authored scene, assets or mixer. EAGAIN
    // retains the unwritten tail; an underrun/suspend attempts preparation
    // without invoking ALSA's potentially sleeping resume recovery.
    class RuntimeAlsaAudioOutput final
    {
        RuntimePcmQueue m_Pcm;
        snd_pcm_t* m_Device=nullptr;
        std::atomic<int> m_Error{0};
        std::atomic<std::uint64_t> m_Accepted{0};
        std::jthread m_Worker;
        static void Check(int status,const char* operation)
        {
            if(status<0)throw std::runtime_error(std::string(operation)+": "+snd_strerror(status));
        }
        void Close() noexcept
        {
            if(m_Worker.joinable()){m_Worker.request_stop();m_Worker.join();}
            if(m_Device){(void)snd_pcm_drop(m_Device);(void)snd_pcm_close(m_Device);m_Device=nullptr;}
        }
        void Run(std::stop_token stop) noexcept
        {
            RuntimePcmPump<> pump(m_Pcm);
            try
            {
                while(!stop.stop_requested()&&!Failed())
                {
                    const auto accepted=pump.Step([&](std::span<const float> pending)->std::size_t
                    {
                        const auto result=snd_pcm_writei(m_Device,pending.data(),static_cast<snd_pcm_uframes_t>(pending.size()/2));
                        if(result>=0)return static_cast<std::size_t>(result);
                        if(result==-EAGAIN||result==-EINTR)return 0;
                        if(result==-EPIPE||result==-ESTRPIPE)
                        {
                            const int prepared=snd_pcm_prepare(m_Device);
                            if(prepared<0)m_Error.store(prepared,std::memory_order_relaxed);
                            return 0;
                        }
                        m_Error.store(static_cast<int>(result),std::memory_order_relaxed);return 0;
                    });
                    m_Accepted.fetch_add(accepted,std::memory_order_relaxed);
                    if(!accepted)std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }
            catch(...){m_Error.store(-EINVAL,std::memory_order_relaxed);}
        }
    public:
        explicit RuntimeAlsaAudioOutput(std::uint32_t rate,const char* device="default")
        {
            if(rate<8000||rate>192000||!device||!*device)
                throw std::invalid_argument("Invalid ALSA output configuration.");
            try
            {
                Check(snd_pcm_open(&m_Device,device,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK),"ALSA open");
                Check(snd_pcm_set_params(m_Device,SND_PCM_FORMAT_FLOAT,SND_PCM_ACCESS_RW_INTERLEAVED,
                    2,rate,1,40000),"ALSA configure stereo float PCM");
                Check(snd_pcm_nonblock(m_Device,1),"ALSA nonblocking output");
                m_Worker=std::jthread([this](std::stop_token stop){Run(stop);});
            }
            catch(...){Close();throw;}
        }
        ~RuntimeAlsaAudioOutput(){Close();}
        RuntimeAlsaAudioOutput(const RuntimeAlsaAudioOutput&)=delete;
        RuntimeAlsaAudioOutput& operator=(const RuntimeAlsaAudioOutput&)=delete;
        [[nodiscard]] bool Failed() const noexcept{return m_Error.load(std::memory_order_relaxed)!=0;}
        [[nodiscard]] std::uint64_t AcceptedFrames() const noexcept{return m_Accepted.load(std::memory_order_relaxed);}
        std::size_t Push(std::span<const float> samples){return m_Pcm.Push(samples);}
    };
}
