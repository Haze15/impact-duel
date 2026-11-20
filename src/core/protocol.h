#pragma once
#include "core/sim.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fg {

constexpr uint32_t NET_MAGIC = 0x46474854; // "FGHT"
constexpr uint8_t NET_VERSION = 1;
constexpr uint16_t DEFAULT_PORT = 7777;
constexpr int NAME_LEN = 16;

enum PacketType : uint8_t {
    PT_HELLO = 1,   // client -> serveur : je veux jouer (perso, pseudo)
    PT_WELCOME = 2, // serveur -> client : ton slot
    PT_FULL = 3,    // serveur -> client : salle pleine
    PT_INPUT = 4,   // client -> serveur : entrées
    PT_STATE = 5,   // serveur -> client : état complet du match
    PT_BYE = 6,     // déconnexion
};

class Writer {
public:
    std::vector<uint8_t> buf;
    void u8(uint8_t v) { buf.push_back(v); }
    void u16(uint16_t v) { u8(uint8_t(v)); u8(uint8_t(v >> 8)); }
    void u32(uint32_t v) { u16(uint16_t(v)); u16(uint16_t(v >> 16)); }
    void i16(int v) { u16(static_cast<uint16_t>(static_cast<int16_t>(v))); }
    void i32(int v) { u32(static_cast<uint32_t>(v)); }
    void f32(float v);
    void str(const std::string& s, int len);
};

class Reader {
public:
    Reader(const uint8_t* p, size_t n) : p_(p), n_(n) {}
    bool ok() const { return ok_; }
    uint8_t u8() { if (i_ + 1 > n_) { ok_ = false; return 0; } return p_[i_++]; }
    uint16_t u16() { uint16_t a = u8(); return uint16_t(a | (uint16_t(u8()) << 8)); }
    uint32_t u32() { uint32_t a = u16(); return a | (uint32_t(u16()) << 16); }
    int i16() { return static_cast<int16_t>(u16()); }
    int i32() { return static_cast<int32_t>(u32()); }
    float f32();
    std::string str(int len);
private:
    const uint8_t* p_;
    size_t n_;
    size_t i_ = 0;
    bool ok_ = true;
};

void writeHeader(Writer& w, PacketType t);
bool readHeader(Reader& r, PacketType& t);

void writeMatch(Writer& w, const Match& m);
bool readMatch(Reader& r, Match& m);

struct InputPacket {
    uint32_t seq = 0;
    uint8_t held = 0;
    uint8_t pressCount[4] = {0, 0, 0, 0}; // compteurs d'appuis A,B,C,D (robuste aux pertes)
};

struct StatePacket {
    uint32_t serverFrame = 0;
    uint8_t connected = 0; // bit 0 = joueur 1, bit 1 = joueur 2
    std::string names[2];
    Match match;
};

std::vector<uint8_t> makeHello(int charId, const std::string& name);
std::vector<uint8_t> makeWelcome(int slot);
std::vector<uint8_t> makeSimple(PacketType t);
std::vector<uint8_t> makeInput(const InputPacket& p);
std::vector<uint8_t> makeState(const StatePacket& s);

bool parseHello(Reader& r, int& charId, std::string& name);
bool parseInput(Reader& r, InputPacket& p);
bool parseState(Reader& r, StatePacket& s);

} // namespace fg
