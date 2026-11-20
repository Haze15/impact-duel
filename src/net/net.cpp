#include "net/net.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socklen_t = int;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSESOCK ::close
#endif

#ifdef _WIN32
#define SockCast static_cast<SOCKET>
#else
#define SockCast static_cast<int>
#endif
#include <cstdlib>
#include <cstring>

namespace net {

bool init() {
#ifdef _WIN32
    WSADATA d;
    return WSAStartup(MAKEWORD(2, 2), &d) == 0;
#else
    return true;
#endif
}

void shutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

std::string Address::toString() const {
    char buf[64];
    in_addr a;
    a.s_addr = ip;
    inet_ntop(AF_INET, &a, buf, sizeof(buf));
    return std::string(buf) + ":" + std::to_string(ntohs(port));
}

bool resolve(const std::string& hostPort, uint16_t defaultPort, Address& out) {
    std::string host = hostPort;
    uint16_t port = defaultPort;
    const auto colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        host = hostPort.substr(0, colon);
        const int p = std::atoi(hostPort.c_str() + colon + 1);
        if (p > 0 && p < 65536) port = static_cast<uint16_t>(p);
    }
    if (host.empty()) host = "127.0.0.1";
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
    const auto* sin = reinterpret_cast<const sockaddr_in*>(res->ai_addr);
    out.ip = sin->sin_addr.s_addr;
    out.port = htons(port);
    freeaddrinfo(res);
    return true;
}

UdpSocket::~UdpSocket() { close(); }

bool UdpSocket::open(uint16_t port) {
    close();
    auto s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) return false;
#else
    if (s < 0) return false;
#endif
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        CLOSESOCK(s);
        return false;
    }
#ifdef _WIN32
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
    handle_ = static_cast<intptr_t>(s);
    return true;
}

void UdpSocket::close() {
    if (handle_ != invalid()) {
        CLOSESOCK(SockCast(handle_));
        handle_ = invalid();
    }
}

bool UdpSocket::send(const Address& to, const void* data, size_t len) {
    if (!isOpen()) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = to.ip;
    addr.sin_port = to.port;
    const long long n = ::sendto(SockCast(handle_), static_cast<const char*>(data),
                                 static_cast<int>(len), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    return n == static_cast<long long>(len);
}

int UdpSocket::receive(Address& from, void* data, size_t maxLen) {
    if (!isOpen()) return 0;
    sockaddr_in addr{};
    socklen_t alen = sizeof(addr);
    const auto n = ::recvfrom(SockCast(handle_), static_cast<char*>(data),
                              static_cast<int>(maxLen), 0, reinterpret_cast<sockaddr*>(&addr), &alen);
    if (n <= 0) return 0;
    from.ip = addr.sin_addr.s_addr;
    from.port = addr.sin_port;
    return static_cast<int>(n);
}

} // namespace net
