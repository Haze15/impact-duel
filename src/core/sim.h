#pragma once
#include "core/characters.h"
#include "core/types.h"

namespace fg {

enum class FState : uint8_t { Idle, Walk, Crouch, Jump, Attack, Hitstun, Blockstun, Knockdown, KO, Victory };
enum class Phase : uint8_t { Waiting, Intro, Fight, RoundEnd, MatchEnd };

struct Fighter {
    int charId = 0;
    float x = 0, y = 0, vx = 0, vy = 0;
    int facing = 1;           // +1 regarde à droite, -1 à gauche
    int health = 0;
    int meter = 0;
    FState state = FState::Idle;
    int stateFrame = 0;
    int move = -1;            // MoveSlot en cours
    int hitsDone = 0;
    int lastHitFrame = -100;
    int stun = 0;
    int airJumps = 0;
    int invuln = 0;
    int combo = 0;            // coups encaissés dans le combo en cours
    int comboDamage = 0;
    uint8_t held = 0;         // entrées maintenues (pour la garde)
    uint8_t buffer = 0;       // tampon d'appuis
    int bufferTimer = 0;
    bool crouching = false;
    bool launched = false;    // en l'air suite à un coup
    bool knockFlag = false;   // tombera au sol à l'atterrissage
    bool helpless = false;    // ne peut plus agir avant d'atterrir
};

struct Projectile {
    bool active = false;
    int owner = 0;
    int charId = 0;
    int slot = 0;
    float x = 0, y = 0, vx = 0, vy = 0, w = 0, h = 0;
    int life = 0;
    int hitsLeft = 0;
    int cooldown = 0;
    int age = 0;
};

struct Spark {
    float x = 0, y = 0;
    int type = 0; // 0 touche, 1 garde, 2 gros coup, 3 annulation projectile, 4 K.O.
    int age = 999;
    uint32_t seq = 0;
};

constexpr int MAX_PROJ = 8;
constexpr int MAX_SPARKS = 12;

struct Match {
    Fighter f[2];
    Projectile proj[MAX_PROJ];
    Spark sparks[MAX_SPARKS];
    uint32_t sparkSeq = 0;
    Phase phase = Phase::Waiting;
    int phaseTimer = 0;
    int roundTime = ROUND_TIME;
    int round = 1;
    int wins[2] = {0, 0};
    int roundWinner = -1;  // 0, 1, 2 = égalité
    bool timeOut = false;
    int hitstop = 0;
    uint32_t frame = 0;
    bool training = false; // mode entraînement : vie/jauge régénérées
};

void initMatch(Match& m, int char0, int char1);
void startRound(Match& m);
void stepMatch(Match& m, const Input in[2]);

Box hurtbox(const Fighter& f);
bool isInvulnerable(const Fighter& f);
Box moveHitboxWorld(const Fighter& f, const MoveDef& mv);
bool moveIsActive(const Fighter& f, const MoveDef& mv);

} // namespace fg
