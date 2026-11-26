#pragma once
#include "RuntimeNetworkProtocol.hpp"
#include <cerrno>
#include <chrono>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace kairo::player::detail
{
    // Socket ownership is process-local and move-only. This adapter binds
    // exclusively to IPv4 loopback: it is a teaching/acceptance transport,
    // not an internet-facing authenticated multiplayer service.
#ifdef _WIN32
    using NativeNetworkSocket=SOCKET;
    inline constexpr NativeNetworkSocket InvalidNetworkSocket=INVALID_SOCKET;
    using NativeAddressLength=int;
#else
    using NativeNetworkSocket=int;
    inline constexpr NativeNetworkSocket InvalidNetworkSocket=-1;
    using NativeAddressLength=socklen_t;
#endif
    class NetworkSocketSession
    {
    public:
        // Winsock requires process initialization; POSIX does not. Keep this
        // object alive until every owned socket has been destroyed.
        NetworkSocketSession()
        {
#ifdef _WIN32
            WSADATA data{};if(WSAStartup(MAKEWORD(2,2),&data)!=0)throw std::runtime_error("WSAStartup failed.");
#endif
        }
        ~NetworkSocketSession()
        {
#ifdef _WIN32
            WSACleanup();
#endif
        }
        NetworkSocketSession(const NetworkSocketSession&)=delete;
        NetworkSocketSession& operator=(const NetworkSocketSession&)=delete;
    };
    class LoopbackTcp
    {
    public:
        explicit LoopbackTcp(NativeNetworkSocket socket):m_Socket(socket)
        {if(socket==InvalidNetworkSocket)throw std::runtime_error("Socket creation failed.");}
        ~LoopbackTcp(){Close();}
        LoopbackTcp(const LoopbackTcp&)=delete;
        LoopbackTcp& operator=(const LoopbackTcp&)=delete;
        LoopbackTcp(LoopbackTcp&& other) noexcept:m_Socket(std::exchange(other.m_Socket,InvalidNetworkSocket)){}
        // Port zero requests an OS-selected free port; GetPort reports it.
        [[nodiscard]] static LoopbackTcp Listen(std::uint16_t port=0)
        {
            LoopbackTcp s(::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));
            const auto a=Address(port);
            if(::bind(s.m_Socket,reinterpret_cast<const sockaddr*>(&a),sizeof(a))!=0||::listen(s.m_Socket,1)!=0)
                throw std::runtime_error("Loopback bind/listen failed.");
            return s;
        }
        [[nodiscard]] std::uint16_t GetPort() const
        {
            sockaddr_in a{};NativeAddressLength length=sizeof(a);
            if(::getsockname(m_Socket,reinterpret_cast<sockaddr*>(&a),&length)!=0)throw std::runtime_error("Port query failed.");
            return ntohs(a.sin_port);
        }
        // Bounded wait for one peer. POSIX poll avoids FD_SET's fixed-size
        // descriptor array; Winsock select stores socket values instead.
        [[nodiscard]] LoopbackTcp Accept() const
        {
#ifdef _WIN32
            fd_set set;FD_ZERO(&set);FD_SET(m_Socket,&set);timeval wait{5,0};
            const int ready=::select(0,&set,nullptr,nullptr,&wait);
#else
            pollfd descriptor{m_Socket,POLLIN,0};
            const int ready=::poll(&descriptor,1,5000);
#endif
            if(ready!=1)throw std::runtime_error("Loopback accept timeout/failure.");
            LoopbackTcp peer(::accept(m_Socket,nullptr,nullptr));peer.Configure();return peer;
        }
        [[nodiscard]] static LoopbackTcp Connect(std::uint16_t port)
        {
            if(!port)throw std::invalid_argument("Invalid loopback port.");
            LoopbackTcp s(::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));
            const auto a=Address(port);
            if(::connect(s.m_Socket,reinterpret_cast<const sockaddr*>(&a),sizeof(a))!=0)
                throw std::runtime_error("Loopback connect failed.");
            s.Configure();return s;
        }
        // TCP is a byte stream, not a message transport. Each partial call
        // consumes the same monotonic frame deadline. A failed frame leaves
        // the stream unusable for further framing; the caller closes it.
        void Send(NetworkFrame frame,std::chrono::milliseconds budget=std::chrono::seconds(3)) const
        {
            const auto bytes=EncodeNetworkFrame(frame);std::size_t done=0;
            const auto deadline=FrameDeadline(budget);
            while(done<bytes.size())
            {
                SetRemainingTimeout(SO_SNDTIMEO,deadline);
#ifdef MSG_NOSIGNAL
                constexpr int flags=MSG_NOSIGNAL;
#else
                constexpr int flags=0;
#endif
                const auto n=::send(m_Socket,reinterpret_cast<const char*>(bytes.data()+done),static_cast<int>(bytes.size()-done),flags);
                if(n<=0)throw std::runtime_error("Loopback send failed.");
                done+=static_cast<std::size_t>(n);
            }
            CheckDeadline(deadline);
        }
        [[nodiscard]] NetworkFrame Receive(std::chrono::milliseconds budget=std::chrono::seconds(3)) const
        {
            NetworkBytes bytes{};std::size_t done=0;
            const auto deadline=FrameDeadline(budget);
            while(done<bytes.size())
            {
                SetRemainingTimeout(SO_RCVTIMEO,deadline);
                const auto n=::recv(m_Socket,reinterpret_cast<char*>(bytes.data()+done),static_cast<int>(bytes.size()-done),0);
                if(n<=0)throw std::runtime_error("Loopback receive failed/closed.");
                done+=static_cast<std::size_t>(n);
            }
            CheckDeadline(deadline);return DecodeNetworkFrame(bytes);
        }
        // Cancel blocking I/O without changing the handle's ownership.
        // A GUI worker must join before the owning socket is destroyed.
        void Shutdown() const noexcept
        {
#ifdef _WIN32
            (void)::shutdown(m_Socket,SD_BOTH);
#else
            (void)::shutdown(m_Socket,SHUT_RDWR);
#endif
        }
    private:
        NativeNetworkSocket m_Socket;
        using Clock=std::chrono::steady_clock;
        static Clock::time_point FrameDeadline(std::chrono::milliseconds budget)
        {
            if(budget.count()<1||budget.count()>30000)throw std::invalid_argument("Invalid network frame deadline.");
            return Clock::now()+budget;
        }
        static void CheckDeadline(Clock::time_point deadline)
        {if(Clock::now()>=deadline)throw std::runtime_error("Loopback frame deadline exceeded.");}
        // Round up rather than sending a zero timeout (which means infinite).
        // Socket timing is OS-granular; the final monotonic check rejects late
        // completion even if a syscall returned data after its deadline.
        void SetRemainingTimeout(int option,Clock::time_point deadline) const
        {
            const auto remaining=std::chrono::ceil<std::chrono::milliseconds>(deadline-Clock::now());
            if(remaining.count()<=0)throw std::runtime_error("Loopback frame deadline exceeded.");
#ifdef _WIN32
            const DWORD timeout=static_cast<DWORD>(remaining.count());
#else
            const timeval timeout{
                static_cast<decltype(timeval::tv_sec)>(remaining.count()/1000),
                static_cast<decltype(timeval::tv_usec)>((remaining.count()%1000)*1000)
            };
#endif
            if(::setsockopt(m_Socket,SOL_SOCKET,option,reinterpret_cast<const char*>(&timeout),static_cast<NativeAddressLength>(sizeof(timeout)))!=0)
                throw std::runtime_error("Cannot configure socket timeout.");
        }
        static sockaddr_in Address(std::uint16_t port)
        {
            sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);return a;
        }
        // On macOS suppress SIGPIPE on broken connections; Linux does this
        // per send with MSG_NOSIGNAL. Frame timeouts are set on each operation.
        void Configure()
        {
#ifdef SO_NOSIGPIPE
            const int enabled=1;
            if(::setsockopt(m_Socket,SOL_SOCKET,SO_NOSIGPIPE,reinterpret_cast<const char*>(&enabled),sizeof(enabled))!=0)
                throw std::runtime_error("Cannot configure socket SIGPIPE policy.");
#endif
        }
        void Close() noexcept
        {
            if(m_Socket==InvalidNetworkSocket)return;
#ifdef _WIN32
            ::closesocket(m_Socket);
#else
            ::close(m_Socket);
#endif
            m_Socket=InvalidNetworkSocket;
        }
    };
}
