#pragma once
#include "RuntimePcmQueue.hpp"
#include "RuntimePcm16.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <string>
#include <thread>

namespace kairo::player::detail
{
    // WinMM signals an event when a prepared buffer completes. Only this
    // worker submits audio; the producer owns the scene and mixer.
    // Buffer storage remains alive until reset/unprepare/close completes.
    class RuntimeWindowsAudioOutput final
    {
        static constexpr std::size_t FramesPerBuffer=512;
        struct Buffer
        {
            std::array<std::int16_t,FramesPerBuffer*2> Samples{};
            WAVEHDR Header{};
            bool Prepared=false,Queued=false;
        };
        RuntimePcmQueue m_Pcm;
        std::array<Buffer,3> m_Buffers;
        HANDLE m_Event=nullptr;
        HWAVEOUT m_Device=nullptr;
        std::atomic<MMRESULT> m_Error{MMSYSERR_NOERROR};
        std::jthread m_Worker;
        static void Check(MMRESULT status,const char* operation)
        {
            if(status!=MMSYSERR_NOERROR)
                throw std::runtime_error(std::string(operation)+": "+std::to_string(status));
        }
        void Close() noexcept
        {
            if(m_Worker.joinable())
            {
                m_Worker.request_stop();if(m_Event)(void)SetEvent(m_Event);m_Worker.join();
            }
            if(m_Device)
            {
                (void)waveOutReset(m_Device);
                for(auto& buffer:m_Buffers)if(buffer.Prepared)
                {
                    (void)waveOutUnprepareHeader(m_Device,&buffer.Header,static_cast<UINT>(sizeof(WAVEHDR)));
                    buffer.Prepared=false;
                }
                (void)waveOutClose(m_Device);m_Device=nullptr;
            }
            if(m_Event){(void)CloseHandle(m_Event);m_Event=nullptr;}
        }
        void Run(std::stop_token stop) noexcept
        {
            std::array<float,FramesPerBuffer*2> pcm{};
            try
            {
                while(!stop.stop_requested()&&!Failed())
                {
                    bool submitted=false;
                    for(auto& buffer:m_Buffers)
                    {
                        if(stop.stop_requested())break;
                        if(buffer.Queued&&!(buffer.Header.dwFlags&WHDR_DONE))continue;
                        (void)m_Pcm.Pop(pcm);
                        ConvertRuntimePcm16(pcm,buffer.Samples);
                        const auto result=waveOutWrite(m_Device,&buffer.Header,static_cast<UINT>(sizeof(WAVEHDR)));
                        if(result!=MMSYSERR_NOERROR)
                        {
                            m_Error.store(result,std::memory_order_relaxed);return;
                        }
                        buffer.Queued=true;submitted=true;
                    }
                    if(!submitted&&!stop.stop_requested())
                    {
                        const auto result=WaitForSingleObject(m_Event,10);
                        if(result==WAIT_FAILED){m_Error.store(MMSYSERR_ERROR,std::memory_order_relaxed);return;}
                    }
                }
            }
            catch(...){m_Error.store(MMSYSERR_ERROR,std::memory_order_relaxed);}
        }
    public:
        explicit RuntimeWindowsAudioOutput(std::uint32_t rate)
        {
            if(rate<8000||rate>192000)throw std::invalid_argument("Invalid Windows audio sample rate.");
            try
            {
                m_Event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
                if(!m_Event)throw std::runtime_error("WinMM completion event creation failed.");
                WAVEFORMATEX format{};
                format.wFormatTag=WAVE_FORMAT_PCM;
                format.nChannels=2;
                format.nSamplesPerSec=rate;
                format.wBitsPerSample=16;
                format.nBlockAlign=4;
                format.nAvgBytesPerSec=rate*4;
                Check(waveOutOpen(&m_Device,WAVE_MAPPER,&format,reinterpret_cast<DWORD_PTR>(m_Event),0,CALLBACK_EVENT),"waveOutOpen");
                for(auto& buffer:m_Buffers)
                {
                    buffer.Header.lpData=reinterpret_cast<LPSTR>(buffer.Samples.data());
                    buffer.Header.dwBufferLength=static_cast<DWORD>(buffer.Samples.size()*sizeof(std::int16_t));
                    Check(waveOutPrepareHeader(m_Device,&buffer.Header,static_cast<UINT>(sizeof(WAVEHDR))),"waveOutPrepareHeader");
                    buffer.Prepared=true;
                }
                m_Worker=std::jthread([this](std::stop_token stop){Run(stop);});
            }
            catch(...){Close();throw;}
        }
        ~RuntimeWindowsAudioOutput(){Close();}
        RuntimeWindowsAudioOutput(const RuntimeWindowsAudioOutput&)=delete;
        RuntimeWindowsAudioOutput& operator=(const RuntimeWindowsAudioOutput&)=delete;
        [[nodiscard]] bool Failed() const noexcept{return m_Error.load(std::memory_order_relaxed)!=MMSYSERR_NOERROR;}
        std::size_t Push(std::span<const float> samples){return m_Pcm.Push(samples);}
    };
}
