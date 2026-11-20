#pragma once
// Petite couche UDP multiplateforme (Windows / Linux / macOS).
// Volontairement sans inclure winsock ici pour éviter les conflits avec raylib.
#include <cstddef>
#include <cstdint>
#include <string>

namespace net {

bool init();     // à appeler une fois (WSAStartup sous Windows)
void shutdown();

struct Address {
    uint32_t ip = 0;   // ordre réseau
    uint16_t port = 0; // ordre réseau
    bool operator==(const Address& o) const { return ip == o.ip && port == o.port; }
    bool operator!=(const Address& o) const { return !(*this == o); }
    std::string toString() const;
};

// "hote:port" ou "hote" (port par défaut)
bool resolve(const std::string& hostPort, uint16_t defaultPort, Address& out);

class UdpSocket {
public:
    UdpSocket() = default;
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    bool open(uint16_t port = 0); // 0 = port quelconque (client)
    void close();
    bool isOpen() const { return handle_ != invalid(); }
    bool send(const Address& to, const void* data, size_t len);
    // non bloquant : renvoie le nombre d'octets, 0 si rien
    int receive(Address& from, void* data, size_t maxLen);

private:
    static intptr_t invalid() { return -1; }
    intptr_t handle_ = -1;
};

} // namespace net
