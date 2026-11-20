// Tests sans affichage : simulation, coups, réseau (sérialisation).
#include "core/protocol.h"
#include "core/sim.h"

#include <cstdio>
#include <cstdlib>

using namespace fg;

static int g_fail = 0;
#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::printf("ECHEC %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
            ++g_fail;                                                          \
        }                                                                      \
    } while (0)

static void skipIntro(Match& m) {
    Input none[2]{};
    while (m.phase == Phase::Intro) stepMatch(m, none);
}

static void run(Match& m, int frames, Input a = {}, Input b = {}) {
    for (int i = 0; i < frames; ++i) {
        Input in[2] = {a, b};
        stepMatch(m, in);
        a.pressed = b.pressed = 0;
    }
}

static void testJabHits() {
    Match m;
    initMatch(m, 0, 0);
    skipIntro(m);
    m.f[0].x = 760; m.f[1].x = 840;
    const int hp = m.f[1].health;
    run(m, 20, {IN_A, IN_A});
    CHECK(m.f[1].health < hp);
    CHECK(m.f[0].meter > 0);
}

static void testBlock() {
    Match m;
    initMatch(m, 0, 0);
    skipIntro(m);
    m.f[0].x = 760; m.f[1].x = 840;
    const int hp = m.f[1].health;
    // J2 regarde à gauche : maintenir droite = garde
    Input atk{IN_B, IN_B}, def{IN_RIGHT, 0};
    for (int i = 0; i < 25; ++i) {
        Input in[2] = {atk, def};
        stepMatch(m, in);
        atk.pressed = 0;
    }
    CHECK(m.f[1].health == hp); // coup normal paré : pas de dégâts
}

static void testLowBeatsStandingBlock() {
    Match m;
    initMatch(m, 0, 0);
    skipIntro(m);
    m.f[0].x = 760; m.f[1].x = 840;
    const int hp = m.f[1].health;
    Input atk{static_cast<uint8_t>(IN_DOWN | IN_A), IN_A}, def{IN_RIGHT, 0};
    for (int i = 0; i < 20; ++i) {
        Input in[2] = {atk, def};
        stepMatch(m, in);
        atk.pressed = 0;
    }
    CHECK(m.f[1].health < hp);
}

static void testProjectile() {
    Match m;
    initMatch(m, 0, 1);
    skipIntro(m);
    const int hp = m.f[1].health;
    run(m, 120, {IN_C, IN_C});
    CHECK(m.f[1].health < hp);
}

static void testGrabAndSuper() {
    Match m;
    initMatch(m, 1, 0); // Brakka
    skipIntro(m);
    m.f[0].x = 760; m.f[1].x = 840;
    int hp = m.f[1].health;
    run(m, 15, {IN_C, IN_C}, {IN_RIGHT, 0}); // saisie malgré la garde
    CHECK(m.f[1].health < hp);
    run(m, 120);
    m.f[0].meter = METER_MAX;
    m.f[0].x = 760; m.f[1].x = 840;
    hp = m.f[1].health;
    run(m, 20, {IN_D, IN_D});
    CHECK(m.f[0].meter < METER_MAX);
    CHECK(hp - m.f[1].health >= 300);
}

static void testTeleport() {
    Match m;
    initMatch(m, 2, 0); // Nyx
    skipIntro(m);
    const float before = m.f[0].x;
    run(m, 30, {static_cast<uint8_t>(IN_DOWN | IN_C), IN_C});
    CHECK(before < m.f[1].x);
    CHECK(m.f[0].x > m.f[1].x); // passé derrière
}

static void testFullMatchAndKO() {
    Match m;
    initMatch(m, 3, 2);
    int guard = 0;
    while (m.phase != Phase::MatchEnd && guard++ < 60 * 60 * 20) {
        Input in[2]{};
        // J1 avance et frappe, J2 ne fait rien
        in[0].held = static_cast<uint8_t>(m.f[0].x < m.f[1].x ? IN_RIGHT : IN_LEFT);
        const float d = m.f[0].x < m.f[1].x ? m.f[1].x - m.f[0].x : m.f[0].x - m.f[1].x;
        if (d < 130 && m.frame % 9 == 0) in[0].pressed = (m.frame / 9) % 3 == 0 ? IN_B : IN_A;
        stepMatch(m, in);
    }
    CHECK(m.phase == Phase::MatchEnd);
    CHECK(m.wins[0] == ROUNDS_TO_WIN);
}

static void testRandomFuzzAndSerialization() {
    srand(1234);
    for (int game = 0; game < 16; ++game) {
        Match m;
        initMatch(m, game % 4, (game / 4) % 4);
        for (int i = 0; i < 60 * 120; ++i) {
            Input in[2];
            for (auto& x : in) {
                x.held = static_cast<uint8_t>(rand() & IN_DIRS);
                if ((rand() & IN_DIRS) == (IN_LEFT | IN_RIGHT)) x.held &= ~IN_LEFT;
                x.pressed = (rand() % 6 == 0) ? static_cast<uint8_t>(IN_A << (rand() % 4)) : 0;
                x.held |= x.pressed;
            }
            stepMatch(m, in);
            for (const auto& f : m.f) {
                CHECK(f.x >= STAGE_LEFT && f.x <= STAGE_RIGHT);
                CHECK(f.y >= 0.f && f.y < 2000.f);
            }
            if (m.phase == Phase::MatchEnd) initMatch(m, (game + 1) % 4, game % 4);
            if (i % 97 == 0) {
                StatePacket sp;
                sp.serverFrame = static_cast<uint32_t>(i);
                sp.connected = 3;
                sp.names[0] = "Alice";
                sp.names[1] = "Bob";
                sp.match = m;
                const auto bytes = makeState(sp);
                Reader r(bytes.data(), bytes.size());
                PacketType t;
                StatePacket back;
                CHECK(readHeader(r, t) && t == PT_STATE);
                CHECK(parseState(r, back));
                CHECK(back.names[1] == "Bob");
                // la simulation continue de façon identique après un aller-retour réseau
                Match a = m, b = back.match;
                for (int k = 0; k < 30; ++k) {
                    Input none[2]{};
                    stepMatch(a, none);
                    stepMatch(b, none);
                }
                CHECK(a.f[0].health == b.f[0].health && a.f[1].health == b.f[1].health);
                CHECK(a.f[0].x == b.f[0].x && a.f[1].x == b.f[1].x);
            }
        }
    }
}

int main() {
    testJabHits();
    testBlock();
    testLowBeatsStandingBlock();
    testProjectile();
    testGrabAndSuper();
    testTeleport();
    testFullMatchAndKO();
    testRandomFuzzAndSerialization();
    if (g_fail) {
        std::printf("%d verification(s) en echec\n", g_fail);
        return 1;
    }
    std::printf("Tous les tests passent.\n");
    return 0;
}
