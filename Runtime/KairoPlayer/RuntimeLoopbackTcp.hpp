#pragma once
#include "RuntimeNetworkProtocol.hpp"
#include <cerrno>
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
        // Bounded wait for one peer. Listener is not used for data frames.
        [[nodiscard]] LoopbackTcp Accept() const
        {
            fd_set set;FD_ZERO(&set);FD_SET(m_Socket,&set);timeval wait{5,0};
#ifdef _WIN32
            const int ready=::select(0,&set,nullptr,nullptr,&wait);
#else
            const int ready=::select(m_Socket+1,&set,nullptr,nullptr,&wait);
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
        // TCP is a byte stream, not a message transport. Loop until the
        // entire fixed-size frame is written/read; EOF or timeout fails.
        void Send(NetworkFrame frame) const
        {
            const auto bytes=EncodeNetworkFrame(frame);std::size_t done=0;
            while(done<bytes.size())
            {
#ifdef MSG_NOSIGNAL
                constexpr int flags=MSG_NOSIGNAL;
#else
                constexpr int flags=0;
#endif
                const auto n=::send(m_Socket,reinterpret_cast<const char*>(bytes.data()+done),static_cast<int>(bytes.size()-done),flags);
                if(n<=0)throw std::runtime_error("Loopback send failed.");
                done+=static_cast<std::size_t>(n);
            }
        }
        [[nodiscard]] NetworkFrame Receive() const
        {
            NetworkBytes bytes{};std::size_t done=0;
            while(done<bytes.size())
            {
                const auto n=::recv(m_Socket,reinterpret_cast<char*>(bytes.data()+done),static_cast<int>(bytes.size()-done),0);
                if(n<=0)throw std::runtime_error("Loopback receive failed/closed.");
                done+=static_cast<std::size_t>(n);
            }
            return DecodeNetworkFrame(bytes);
        }
    private:
        NativeNetworkSocket m_Socket;
        static sockaddr_in Address(std::uint16_t port)
        {
            sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);return a;
        }
        // Three-second per-operation timeouts. On macOS suppress SIGPIPE
        // on broken connections; Linux does this per send with MSG_NOSIGNAL.
        void Configure()
        {
#ifdef _WIN32
            const DWORD timeout=3000;
#else
            const timeval timeout{3,0};
#endif
            for(const int option:{SO_RCVTIMEO,SO_SNDTIMEO})
                if(::setsockopt(m_Socket,SOL_SOCKET,option,reinterpret_cast<const char*>(&timeout),sizeof(timeout))!=0)
                    throw std::runtime_error("Cannot configure socket timeout.");
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
