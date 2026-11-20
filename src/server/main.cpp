// IMPACT DUEL — serveur de match faisant autorité.
// Il simule le combat à 60 Hz, reçoit les entrées des deux joueurs et leur renvoie l'état.
#include "core/protocol.h"
#include "core/sim.h"
#include "net/net.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using namespace fg;
using Clock = std::chrono::steady_clock;

namespace {

std::atomic<bool> g_running{true};
void onSignal(int) { g_running = false; }

struct Client {
    bool used = false;
    net::Address addr;
    std::string name;
    int charId = 0;
    uint32_t lastSeq = 0;
    uint8_t held = 0;
    uint8_t prevHeld = 0;
    uint8_t lastCount[4] = {0, 0, 0, 0};
    uint8_t pendingPress = 0;
    Clock::time_point lastSeen;
};

void log(const char* fmt, const std::string& a = "", const std::string& b = "") {
    std::printf(fmt, a.c_str(), b.c_str());
    std::printf("\n");
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv) {
    uint16_t port = DEFAULT_PORT;
    if (argc > 1) port = static_cast<uint16_t>(std::atoi(argv[1]));
    if (const char* env = std::getenv("PORT"); env && argc <= 1) port = static_cast<uint16_t>(std::atoi(env));

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    if (!net::init()) { std::fprintf(stderr, "Erreur init reseau\n"); return 1; }
    net::UdpSocket sock;
    if (!sock.open(port)) { std::fprintf(stderr, "Impossible d'ouvrir le port UDP %u\n", port); return 1; }
    std::printf("IMPACT DUEL serveur en ecoute sur UDP %u\n", port);
    std::fflush(stdout);

    Client clients[2];
    Match match;
    match.phase = Phase::Waiting;
    uint32_t serverFrame = 0;
    bool matchRunning = false;

    const auto tick = std::chrono::microseconds(1000000 / FPS);
    auto next = Clock::now();
    uint8_t buf[2048];

    while (g_running) {
        const auto now = Clock::now();

        // --- réception
        for (;;) {
            net::Address from;
            const int n = sock.receive(from, buf, sizeof(buf));
            if (n <= 0) break;
            Reader r(buf, static_cast<size_t>(n));
            PacketType type;
            if (!readHeader(r, type)) continue;
            int slot = -1;
            for (int i = 0; i < 2; ++i) if (clients[i].used && clients[i].addr == from) slot = i;

            if (type == PT_HELLO) {
                int charId; std::string name;
                if (!parseHello(r, charId, name)) continue;
                if (slot < 0) {
                    for (int i = 0; i < 2; ++i) if (!clients[i].used) { slot = i; break; }
                    if (slot < 0) {
                        const auto pkt = makeSimple(PT_FULL);
                        sock.send(from, pkt.data(), pkt.size());
                        continue;
                    }
                    Client& c = clients[slot];
                    c = Client{};
                    c.used = true;
                    c.addr = from;
                    c.name = name.empty() ? (slot == 0 ? "Joueur 1" : "Joueur 2") : name;
                    c.charId = charId;
                    c.lastSeen = now;
                    std::printf("[+] %s (%s) rejoint en J%d avec %s\n", c.name.c_str(), from.toString().c_str(), slot + 1,
                                getCharacter(charId).name);
                    std::fflush(stdout);
                }
                clients[slot].lastSeen = now;
                const auto pkt = makeWelcome(slot);
                sock.send(from, pkt.data(), pkt.size());
            } else if (type == PT_INPUT && slot >= 0) {
                InputPacket ip;
                if (!parseInput(r, ip)) continue;
                Client& c = clients[slot];
                c.lastSeen = now;
                if (ip.seq <= c.lastSeq && c.lastSeq - ip.seq < 1000000u) continue; // vieux paquet
                c.lastSeq = ip.seq;
                c.held = ip.held;
                for (int b = 0; b < 4; ++b) {
                    if (static_cast<uint8_t>(ip.pressCount[b] - c.lastCount[b]) > 0)
                        c.pendingPress |= static_cast<uint8_t>(IN_A << b);
                    c.lastCount[b] = ip.pressCount[b];
                }
            } else if (type == PT_BYE && slot >= 0) {
                log("[-] %s quitte la partie", clients[slot].name);
                clients[slot].used = false;
            }
        }

        // --- délais d'expiration
        for (auto& c : clients) {
            if (c.used && now - c.lastSeen > std::chrono::seconds(5)) {
                log("[x] %s ne repond plus", c.name);
                c.used = false;
            }
        }

        // --- gestion du match
        const bool both = clients[0].used && clients[1].used;
        if (!both && matchRunning) {
            log("Match interrompu, en attente d'un joueur...");
            matchRunning = false;
            match = Match{};
            match.phase = Phase::Waiting;
        }
        if (both && !matchRunning) {
            initMatch(match, clients[0].charId, clients[1].charId);
            matchRunning = true;
            std::printf("Match : %s (%s) vs %s (%s)\n", clients[0].name.c_str(), getCharacter(clients[0].charId).name,
                        clients[1].name.c_str(), getCharacter(clients[1].charId).name);
            std::fflush(stdout);
        }
        if (matchRunning && match.phase == Phase::MatchEnd && match.phaseTimer <= 0) {
            log("Revanche !");
            initMatch(match, clients[0].charId, clients[1].charId);
        }

        Input in[2];
        for (int i = 0; i < 2; ++i) {
            Client& c = clients[i];
            if (!c.used) continue;
            in[i].held = c.held;
            in[i].pressed = static_cast<uint8_t>((c.held & ~c.prevHeld & IN_DIRS) | c.pendingPress);
            c.prevHeld = c.held;
            c.pendingPress = 0;
        }
        if (matchRunning) stepMatch(match, in);
        serverFrame++;

        // --- diffusion
        StatePacket sp;
        sp.serverFrame = serverFrame;
        sp.connected = static_cast<uint8_t>((clients[0].used ? 1 : 0) | (clients[1].used ? 2 : 0));
        for (int i = 0; i < 2; ++i) sp.names[i] = clients[i].used ? clients[i].name : "";
        sp.match = match;
        const auto pkt = makeState(sp);
        for (auto& c : clients) if (c.used) sock.send(c.addr, pkt.data(), pkt.size());

        next += tick;
        if (Clock::now() > next + std::chrono::milliseconds(100)) next = Clock::now(); // rattrapage
        std::this_thread::sleep_until(next);
    }

    const auto bye = makeSimple(PT_BYE);
    for (auto& c : clients) if (c.used) sock.send(c.addr, bye.data(), bye.size());
    std::printf("Arret du serveur.\n");
    net::shutdown();
    return 0;
}
