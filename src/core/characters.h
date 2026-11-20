#pragma once
#include "core/types.h"

namespace fg {

enum MoveFlag : uint16_t {
    MF_LOW       = 1 << 0,  // se pare accroupi
    MF_OVERHEAD  = 1 << 1,  // se pare debout
    MF_KNOCKDOWN = 1 << 2,  // fait tomber
    MF_LAUNCH    = 1 << 3,  // envoie en l'air
    MF_GRAB      = 1 << 4,  // saisie (imparable)
    MF_TELEPORT  = 1 << 5,  // téléportation derrière l'adversaire
    MF_AIR_OK    = 1 << 6,  // utilisable en l'air
    MF_NOPUSH    = 1 << 7,  // traverse l'adversaire
};

enum Limb : uint8_t { LIMB_ARM, LIMB_LEG, LIMB_BODY };

struct ProjectileDef {
    bool enabled = false;
    float speed = 0, w = 0, h = 0, yOff = 0;
    float airVy = 0; // vitesse verticale si lancé en l'air
    int life = 0;
    int damage = 0, hitstun = 0, blockstun = 0;
    float kbx = 0, kby = 0;
    uint16_t flags = 0;
    int hits = 1, hitInterval = 1;
};

struct MoveDef {
    const char* name = "";
    const char* input = "";  // texte affiché dans la liste de coups
    Limb limb = LIMB_ARM;
    int startup = 1, active = 1, recovery = 1;
    int damage = 0, hitstun = 0, blockstun = 0;
    Box hitbox{};            // relatif : x vers l'avant depuis le centre, y depuis les pieds
    float kbx = 0, kby = 0;  // recul infligé (x vers l'avant de l'attaquant)
    uint16_t flags = 0;
    int hits = 1, hitInterval = 1;
    float moveVx = 0, moveVy = 0; // vitesse propre pendant le coup
    int moveStart = -1, moveEnd = -1;
    int invulnStart = 0, invulnEnd = 0; // invulnérable sur [start, end)
    int meterCost = 0;
    ProjectileDef proj{};
    bool cancelable = false; // annulable en spécial/super si touche
    int total() const { return startup + active + recovery; }
};

enum MoveSlot : int8_t {
    M_LIGHT, M_HEAVY, M_CR_LIGHT, M_CR_HEAVY, M_AIR_LIGHT, M_AIR_HEAVY,
    M_SP_NEUTRAL, M_SP_FORWARD, M_SP_DOWN, M_SUPER, M_COUNT
};
inline bool isSpecialSlot(int s) { return s >= M_SP_NEUTRAL; }
inline bool isCrouchSlot(int s) { return s == M_CR_LIGHT || s == M_CR_HEAVY; }
inline bool isAirSlot(int s) { return s == M_AIR_LIGHT || s == M_AIR_HEAVY; }

struct CharacterDef {
    const char* name;
    const char* title;
    const char* desc;
    uint32_t color;   // RGBA principal
    uint32_t color2;  // RGBA secondaire
    uint32_t accent;  // effets
    int maxHealth;
    float walkSpeed, backSpeed, jumpVel, jumpVx;
    int airJumps;
    float width, height, thickness;
    int statPower, statSpeed, statRange; // 1..5, pour l'écran de sélection
    MoveDef moves[M_COUNT];
};

int characterCount();
const CharacterDef& getCharacter(int id);

} // namespace fg
