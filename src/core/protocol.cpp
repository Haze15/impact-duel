#include "core/protocol.h"
#include <cstring>

namespace fg {

void Writer::f32(float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    u32(u);
}

void Writer::str(const std::string& s, int len) {
    for (int i = 0; i < len; ++i) u8(i < static_cast<int>(s.size()) ? static_cast<uint8_t>(s[static_cast<size_t>(i)]) : 0);
}

float Reader::f32() {
    uint32_t u = u32();
    float v;
    std::memcpy(&v, &u, 4);
    return v;
}

std::string Reader::str(int len) {
    std::string s;
    for (int i = 0; i < len; ++i) {
        char c = static_cast<char>(u8());
        if (c != 0 && c >= 32 && c < 127) s.push_back(c);
    }
    return s;
}

void writeHeader(Writer& w, PacketType t) {
    w.u32(NET_MAGIC);
    w.u8(NET_VERSION);
    w.u8(t);
}

bool readHeader(Reader& r, PacketType& t) {
    if (r.u32() != NET_MAGIC) return false;
    if (r.u8() != NET_VERSION) return false;
    t = static_cast<PacketType>(r.u8());
    return r.ok();
}

static void writeFighter(Writer& w, const Fighter& f) {
    w.u8(static_cast<uint8_t>(f.charId));
    w.f32(f.x); w.f32(f.y); w.f32(f.vx); w.f32(f.vy);
    w.u8(static_cast<uint8_t>(f.facing > 0 ? 1 : 0));
    w.i16(f.health); w.i16(f.meter);
    w.u8(static_cast<uint8_t>(f.state));
    w.i16(f.stateFrame);
    w.u8(static_cast<uint8_t>(f.move + 1));
    w.u8(static_cast<uint8_t>(f.hitsDone));
    w.i16(f.lastHitFrame);
    w.i16(f.stun);
    w.u8(static_cast<uint8_t>(f.airJumps));
    w.u8(static_cast<uint8_t>(f.invuln));
    w.u8(static_cast<uint8_t>(f.combo));
    w.i16(f.comboDamage);
    w.u8(f.held); w.u8(f.buffer); w.u8(static_cast<uint8_t>(f.bufferTimer));
    w.u8(static_cast<uint8_t>((f.crouching ? 1 : 0) | (f.launched ? 2 : 0) | (f.knockFlag ? 4 : 0) | (f.helpless ? 8 : 0)));
}

static void readFighter(Reader& r, Fighter& f) {
    f.charId = r.u8();
    f.x = r.f32(); f.y = r.f32(); f.vx = r.f32(); f.vy = r.f32();
    f.facing = r.u8() ? 1 : -1;
    f.health = r.i16(); f.meter = r.i16();
    f.state = static_cast<FState>(r.u8());
    f.stateFrame = r.i16();
    f.move = static_cast<int>(r.u8()) - 1;
    f.hitsDone = r.u8();
    f.lastHitFrame = r.i16();
    f.stun = r.i16();
    f.airJumps = r.u8();
    f.invuln = r.u8();
    f.combo = r.u8();
    f.comboDamage = r.i16();
    f.held = r.u8(); f.buffer = r.u8(); f.bufferTimer = r.u8();
    const uint8_t b = r.u8();
    f.crouching = b & 1; f.launched = b & 2; f.knockFlag = b & 4; f.helpless = b & 8;
    if (f.charId >= characterCount()) f.charId = 0;
    if (f.move >= M_COUNT) f.move = -1;
    if (static_cast<int>(f.state) > static_cast<int>(FState::Victory)) f.state = FState::Idle;
}

void writeMatch(Writer& w, const Match& m) {
    for (const auto& f : m.f) writeFighter(w, f);
    uint8_t mask = 0;
    for (int i = 0; i < MAX_PROJ; ++i) if (m.proj[i].active) mask |= uint8_t(1u << i);
    w.u8(mask);
    for (const auto& p : m.proj) {
        if (!p.active) continue;
        w.u8(static_cast<uint8_t>(p.owner)); w.u8(static_cast<uint8_t>(p.charId)); w.u8(static_cast<uint8_t>(p.slot));
        w.f32(p.x); w.f32(p.y); w.f32(p.vx); w.f32(p.vy); w.f32(p.w); w.f32(p.h);
        w.i16(p.life); w.u8(static_cast<uint8_t>(p.hitsLeft)); w.u8(static_cast<uint8_t>(p.cooldown)); w.i16(p.age);
    }
    for (const auto& s : m.sparks) {
        w.f32(s.x); w.f32(s.y); w.u8(static_cast<uint8_t>(s.type)); w.i16(s.age); w.u32(s.seq);
    }
    w.u32(m.sparkSeq);
    w.u8(static_cast<uint8_t>(m.phase));
    w.i16(m.phaseTimer);
    w.i16(m.roundTime);
    w.u8(static_cast<uint8_t>(m.round));
    w.u8(static_cast<uint8_t>(m.wins[0])); w.u8(static_cast<uint8_t>(m.wins[1]));
    w.u8(static_cast<uint8_t>(m.roundWinner + 1));
    w.u8(m.timeOut ? 1 : 0);
    w.u8(static_cast<uint8_t>(m.hitstop));
    w.u32(m.frame);
    w.u8(m.training ? 1 : 0);
}

bool readMatch(Reader& r, Match& m) {
    for (auto& f : m.f) readFighter(r, f);
    const uint8_t mask = r.u8();
    for (int i = 0; i < MAX_PROJ; ++i) {
        Projectile& p = m.proj[i];
        p = Projectile{};
        if (!(mask & (1u << i))) continue;
        p.active = true;
        p.owner = r.u8() & 1; p.charId = r.u8(); p.slot = r.u8();
        p.x = r.f32(); p.y = r.f32(); p.vx = r.f32(); p.vy = r.f32(); p.w = r.f32(); p.h = r.f32();
        p.life = r.i16(); p.hitsLeft = r.u8(); p.cooldown = r.u8(); p.age = r.i16();
        if (p.charId >= characterCount()) p.charId = 0;
        if (p.slot >= M_COUNT) p.slot = M_SP_NEUTRAL;
    }
    for (auto& s : m.sparks) {
        s.x = r.f32(); s.y = r.f32(); s.type = r.u8(); s.age = r.i16(); s.seq = r.u32();
    }
    m.sparkSeq = r.u32();
    m.phase = static_cast<Phase>(r.u8());
    m.phaseTimer = r.i16();
    m.roundTime = r.i16();
    m.round = r.u8();
    m.wins[0] = r.u8(); m.wins[1] = r.u8();
    m.roundWinner = static_cast<int>(r.u8()) - 1;
    m.timeOut = r.u8() != 0;
    m.hitstop = r.u8();
    m.frame = r.u32();
    m.training = r.u8() != 0;
    if (static_cast<int>(m.phase) > static_cast<int>(Phase::MatchEnd)) m.phase = Phase::Waiting;
    return r.ok();
}

std::vector<uint8_t> makeHello(int charId, const std::string& name) {
    Writer w;
    writeHeader(w, PT_HELLO);
    w.u8(static_cast<uint8_t>(charId));
    w.str(name, NAME_LEN);
    return w.buf;
}

std::vector<uint8_t> makeWelcome(int slot) {
    Writer w;
    writeHeader(w, PT_WELCOME);
    w.u8(static_cast<uint8_t>(slot));
    return w.buf;
}

std::vector<uint8_t> makeSimple(PacketType t) {
    Writer w;
    writeHeader(w, t);
    return w.buf;
}

std::vector<uint8_t> makeInput(const InputPacket& p) {
    Writer w;
    writeHeader(w, PT_INPUT);
    w.u32(p.seq);
    w.u8(p.held);
    for (uint8_t c : p.pressCount) w.u8(c);
    return w.buf;
}

std::vector<uint8_t> makeState(const StatePacket& s) {
    Writer w;
    writeHeader(w, PT_STATE);
    w.u32(s.serverFrame);
    w.u8(s.connected);
    w.str(s.names[0], NAME_LEN);
    w.str(s.names[1], NAME_LEN);
    writeMatch(w, s.match);
    return w.buf;
}

bool parseHello(Reader& r, int& charId, std::string& name) {
    charId = r.u8();
    name = r.str(NAME_LEN);
    if (charId >= characterCount()) charId = 0;
    return r.ok();
}

bool parseInput(Reader& r, InputPacket& p) {
    p.seq = r.u32();
    p.held = r.u8();
    for (auto& c : p.pressCount) c = r.u8();
    return r.ok();
}

bool parseState(Reader& r, StatePacket& s) {
    s.serverFrame = r.u32();
    s.connected = r.u8();
    s.names[0] = r.str(NAME_LEN);
    s.names[1] = r.str(NAME_LEN);
    return readMatch(r, s.match) && r.ok();
}

} // namespace fg
