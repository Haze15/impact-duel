#include "core/characters.h"
#include <vector>

namespace fg {

static constexpr uint16_t F(int v) { return static_cast<uint16_t>(v); }

// ---------------------------------------------------------------------------
// KAEL — combattant équilibré (projectile, uppercut, poing tornade)
// ---------------------------------------------------------------------------
static CharacterDef makeKael() {
    CharacterDef c{"KAEL", "Le Poing du Dragon",
                   "Polyvalent. Boule d'energie, uppercut invincible, bon partout.",
                   0xF2F2F2FF, 0xB8B8C8FF, 0xFF4A3AFF,
                   1000, 4.2f, 3.4f, 17.f, 4.6f, 0, 60, 150, 10, 3, 3, 3, {}};
    c.moves[M_LIGHT] = {.name = "Jab", .input = "A", .limb = LIMB_ARM, .startup = 4, .active = 3, .recovery = 7,
                        .damage = 35, .hitstun = 14, .blockstun = 10, .hitbox = {20, 95, 55, 22}, .kbx = 2.5f, .cancelable = true};
    c.moves[M_HEAVY] = {.name = "Direct", .input = "B", .limb = LIMB_ARM, .startup = 8, .active = 4, .recovery = 16,
                        .damage = 85, .hitstun = 20, .blockstun = 16, .hitbox = {20, 90, 75, 28}, .kbx = 5, .cancelable = true};
    c.moves[M_CR_LIGHT] = {.name = "Coup bas", .input = "Bas + A", .limb = LIMB_LEG, .startup = 5, .active = 3, .recovery = 8,
                           .damage = 30, .hitstun = 13, .blockstun = 9, .hitbox = {20, 0, 65, 25}, .kbx = 2, .flags = MF_LOW, .cancelable = true};
    c.moves[M_CR_HEAVY] = {.name = "Balayette", .input = "Bas + B", .limb = LIMB_LEG, .startup = 9, .active = 4, .recovery = 22,
                           .damage = 80, .hitstun = 24, .blockstun = 16, .hitbox = {15, 0, 95, 24}, .kbx = 3, .kby = 7,
                           .flags = F(MF_LOW | MF_KNOCKDOWN)};
    c.moves[M_AIR_LIGHT] = {.name = "Genou saute", .input = "Air + A", .limb = LIMB_LEG, .startup = 4, .active = 6, .recovery = 8,
                            .damage = 45, .hitstun = 15, .blockstun = 10, .hitbox = {10, 50, 50, 40}, .kbx = 2, .flags = MF_OVERHEAD};
    c.moves[M_AIR_HEAVY] = {.name = "Pied plongeant", .input = "Air + B", .limb = LIMB_LEG, .startup = 7, .active = 6, .recovery = 10,
                            .damage = 80, .hitstun = 20, .blockstun = 14, .hitbox = {15, 15, 70, 45}, .kbx = 3, .flags = MF_OVERHEAD};
    c.moves[M_SP_NEUTRAL] = {.name = "Onde de choc", .input = "C", .limb = LIMB_ARM, .startup = 12, .active = 1, .recovery = 26,
                             .proj = {.enabled = true, .speed = 9, .w = 40, .h = 40, .yOff = 85, .life = 130,
                                      .damage = 80, .hitstun = 20, .blockstun = 16, .kbx = 4}};
    c.moves[M_SP_FORWARD] = {.name = "Poing tornade", .input = "Avant + C", .limb = LIMB_ARM, .startup = 10, .active = 12, .recovery = 18,
                             .damage = 100, .hitstun = 22, .blockstun = 16, .hitbox = {20, 70, 60, 40}, .kbx = 7, .kby = 9,
                             .flags = MF_KNOCKDOWN, .moveVx = 9, .moveStart = 8, .moveEnd = 22};
    c.moves[M_SP_DOWN] = {.name = "Uppercut du dragon", .input = "Bas + C", .limb = LIMB_ARM, .startup = 3, .active = 10, .recovery = 28,
                          .damage = 120, .hitstun = 30, .blockstun = 18, .hitbox = {5, 80, 60, 90}, .kbx = 3, .kby = 15,
                          .flags = F(MF_KNOCKDOWN | MF_LAUNCH), .moveVx = 2, .moveVy = 15, .moveStart = 3, .moveEnd = 13,
                          .invulnStart = 0, .invulnEnd = 9};
    c.moves[M_SUPER] = {.name = "Rayon du dragon", .input = "D (jauge pleine)", .limb = LIMB_ARM, .startup = 20, .active = 1, .recovery = 40,
                        .invulnStart = 0, .invulnEnd = 21, .meterCost = SUPER_COST,
                        .proj = {.enabled = true, .speed = 14, .w = 130, .h = 90, .yOff = 85, .life = 100,
                                 .damage = 65, .hitstun = 26, .blockstun = 14, .kbx = 3, .hits = 5, .hitInterval = 5}};
    return c;
}

// ---------------------------------------------------------------------------
// BRAKKA — lutteur lourd (saisies, charge, séisme)
// ---------------------------------------------------------------------------
static CharacterDef makeBrakka() {
    CharacterDef c{"BRAKKA", "La Montagne",
                   "Lent mais enorme. Saisies devastatrices et beaucoup de vie.",
                   0xA0522DFF, 0x6B3A1EFF, 0xFFB020FF,
                   1200, 3.0f, 2.6f, 15.f, 3.5f, 0, 82, 168, 16, 5, 1, 2, {}};
    c.moves[M_LIGHT] = {.name = "Gifle", .input = "A", .limb = LIMB_ARM, .startup = 5, .active = 3, .recovery = 9,
                        .damage = 45, .hitstun = 14, .blockstun = 10, .hitbox = {25, 100, 60, 28}, .kbx = 3, .cancelable = true};
    c.moves[M_HEAVY] = {.name = "Marteau", .input = "B", .limb = LIMB_ARM, .startup = 12, .active = 5, .recovery = 20,
                        .damage = 120, .hitstun = 24, .blockstun = 18, .hitbox = {25, 80, 85, 50}, .kbx = 6, .cancelable = true};
    c.moves[M_CR_LIGHT] = {.name = "Coup de tibia", .input = "Bas + A", .limb = LIMB_LEG, .startup = 6, .active = 3, .recovery = 10,
                           .damage = 40, .hitstun = 13, .blockstun = 10, .hitbox = {25, 0, 60, 28}, .kbx = 2, .flags = MF_LOW, .cancelable = true};
    c.moves[M_CR_HEAVY] = {.name = "Frappe au sol", .input = "Bas + B", .limb = LIMB_ARM, .startup = 14, .active = 5, .recovery = 24,
                           .damage = 110, .hitstun = 26, .blockstun = 18, .hitbox = {20, 0, 110, 35}, .kbx = 3, .kby = 6,
                           .flags = F(MF_LOW | MF_KNOCKDOWN)};
    c.moves[M_AIR_LIGHT] = {.name = "Coude", .input = "Air + A", .limb = LIMB_ARM, .startup = 5, .active = 5, .recovery = 10,
                            .damage = 50, .hitstun = 16, .blockstun = 10, .hitbox = {15, 60, 55, 40}, .kbx = 2, .flags = MF_OVERHEAD};
    c.moves[M_AIR_HEAVY] = {.name = "Bombe corporelle", .input = "Air + B", .limb = LIMB_BODY, .startup = 8, .active = 10, .recovery = 12,
                            .damage = 110, .hitstun = 22, .blockstun = 16, .hitbox = {-30, 0, 100, 60}, .kbx = 3, .kby = 5,
                            .flags = F(MF_OVERHEAD | MF_KNOCKDOWN)};
    c.moves[M_SP_NEUTRAL] = {.name = "Saisie de l'ours", .input = "C (pres)", .limb = LIMB_ARM, .startup = 6, .active = 3, .recovery = 30,
                             .damage = 200, .hitstun = 30, .hitbox = {20, 40, 60, 90}, .kbx = 7, .kby = 12,
                             .flags = F(MF_GRAB | MF_KNOCKDOWN)};
    c.moves[M_SP_FORWARD] = {.name = "Charge du taureau", .input = "Avant + C", .limb = LIMB_BODY, .startup = 12, .active = 16, .recovery = 20,
                             .damage = 110, .hitstun = 24, .blockstun = 14, .hitbox = {25, 30, 50, 110}, .kbx = 10, .kby = 8,
                             .flags = MF_KNOCKDOWN, .moveVx = 11, .moveStart = 10, .moveEnd = 28};
    c.moves[M_SP_DOWN] = {.name = "Seisme", .input = "Bas + C", .limb = LIMB_ARM, .startup = 18, .active = 6, .recovery = 24,
                          .damage = 90, .hitstun = 22, .blockstun = 16, .hitbox = {-60, 0, 260, 30}, .kbx = 2, .kby = 8,
                          .flags = F(MF_LOW | MF_KNOCKDOWN)};
    c.moves[M_SUPER] = {.name = "Suplex titanesque", .input = "D (jauge pleine)", .limb = LIMB_ARM, .startup = 4, .active = 5, .recovery = 40,
                        .damage = 380, .hitstun = 40, .hitbox = {15, 20, 95, 120}, .kbx = -9, .kby = 16,
                        .flags = F(MF_GRAB | MF_KNOCKDOWN), .invulnStart = 0, .invulnEnd = 9, .meterCost = SUPER_COST};
    return c;
}

// ---------------------------------------------------------------------------
// NYX — ninja rapide (double saut, téléportation, shuriken aérien)
// ---------------------------------------------------------------------------
static CharacterDef makeNyx() {
    CharacterDef c{"NYX", "L'Ombre Furtive",
                   "Tres rapide, double saut, teleportation. Peu de vie.",
                   0x7B3FD0FF, 0x3E1F70FF, 0x40F0D0FF,
                   850, 5.5f, 4.5f, 18.f, 5.6f, 1, 54, 140, 9, 2, 5, 2, {}};
    c.moves[M_LIGHT] = {.name = "Taillade", .input = "A", .limb = LIMB_ARM, .startup = 3, .active = 3, .recovery = 6,
                        .damage = 30, .hitstun = 13, .blockstun = 9, .hitbox = {20, 85, 60, 30}, .kbx = 2, .cancelable = true};
    c.moves[M_HEAVY] = {.name = "Lame croisee", .input = "B", .limb = LIMB_ARM, .startup = 7, .active = 4, .recovery = 14,
                        .damage = 40, .hitstun = 19, .blockstun = 14, .hitbox = {20, 75, 80, 40}, .kbx = 2.5f,
                        .hits = 2, .hitInterval = 2, .cancelable = true};
    c.moves[M_CR_LIGHT] = {.name = "Fauchage", .input = "Bas + A", .limb = LIMB_LEG, .startup = 4, .active = 3, .recovery = 7,
                           .damage = 28, .hitstun = 13, .blockstun = 9, .hitbox = {20, 0, 62, 25}, .kbx = 2, .flags = MF_LOW, .cancelable = true};
    c.moves[M_CR_HEAVY] = {.name = "Glissade", .input = "Bas + B", .limb = LIMB_LEG, .startup = 8, .active = 10, .recovery = 16,
                           .damage = 70, .hitstun = 22, .blockstun = 14, .hitbox = {15, 0, 70, 25}, .kbx = 3, .kby = 6,
                           .flags = F(MF_LOW | MF_KNOCKDOWN), .moveVx = 8, .moveStart = 6, .moveEnd = 18};
    c.moves[M_AIR_LIGHT] = {.name = "Pied eclair", .input = "Air + A", .limb = LIMB_LEG, .startup = 3, .active = 6, .recovery = 6,
                            .damage = 35, .hitstun = 14, .blockstun = 9, .hitbox = {10, 40, 55, 40}, .kbx = 2, .flags = MF_OVERHEAD};
    c.moves[M_AIR_HEAVY] = {.name = "Lame tombante", .input = "Air + B", .limb = LIMB_ARM, .startup = 6, .active = 8, .recovery = 10,
                            .damage = 38, .hitstun = 18, .blockstun = 12, .hitbox = {0, 0, 75, 70}, .kbx = 2, .flags = MF_OVERHEAD,
                            .hits = 2, .hitInterval = 4};
    c.moves[M_SP_NEUTRAL] = {.name = "Shuriken", .input = "C (sol ou air)", .limb = LIMB_ARM, .startup = 8, .active = 1, .recovery = 16,
                             .flags = MF_AIR_OK,
                             .proj = {.enabled = true, .speed = 13, .w = 24, .h = 24, .yOff = 90, .airVy = -6, .life = 90,
                                      .damage = 45, .hitstun = 14, .blockstun = 10, .kbx = 2}};
    c.moves[M_SP_FORWARD] = {.name = "Pas de l'ombre", .input = "Avant + C", .limb = LIMB_ARM, .startup = 6, .active = 10, .recovery = 16,
                             .damage = 80, .hitstun = 20, .blockstun = 12, .hitbox = {0, 40, 70, 80}, .kbx = 4,
                             .flags = MF_NOPUSH, .moveVx = 16, .moveStart = 6, .moveEnd = 16, .invulnStart = 4, .invulnEnd = 16};
    c.moves[M_SP_DOWN] = {.name = "Teleportation", .input = "Bas + C", .limb = LIMB_BODY, .startup = 14, .active = 1, .recovery = 14,
                          .flags = MF_TELEPORT, .invulnStart = 0, .invulnEnd = 16};
    c.moves[M_SUPER] = {.name = "Mille lames", .input = "D (jauge pleine)", .limb = LIMB_ARM, .startup = 8, .active = 30, .recovery = 30,
                        .damage = 40, .hitstun = 20, .blockstun = 10, .hitbox = {-20, 20, 140, 110}, .kbx = 1,
                        .flags = MF_NOPUSH, .hits = 8, .hitInterval = 4, .moveVx = 6, .moveStart = 8, .moveEnd = 38,
                        .invulnStart = 0, .invulnEnd = 20, .meterCost = SUPER_COST};
    return c;
}

// ---------------------------------------------------------------------------
// VOLT — zoneur électrique (orbe lent, lance longue portée, pilier anti-air)
// ---------------------------------------------------------------------------
static CharacterDef makeVolt() {
    CharacterDef c{"VOLT", "Le Tisseur d'Orage",
                   "Garde a distance. Longue portee, orbe lent, anti-air puissant.",
                   0x2FB8E8FF, 0x14587AFF, 0xFFF04AFF,
                   900, 3.6f, 3.4f, 16.f, 4.f, 0, 58, 155, 10, 3, 2, 5, {}};
    c.moves[M_LIGHT] = {.name = "Decharge", .input = "A", .limb = LIMB_ARM, .startup = 5, .active = 3, .recovery = 8,
                        .damage = 32, .hitstun = 14, .blockstun = 10, .hitbox = {20, 90, 70, 22}, .kbx = 2.5f, .cancelable = true};
    c.moves[M_HEAVY] = {.name = "Baton foudre", .input = "B", .limb = LIMB_ARM, .startup = 10, .active = 5, .recovery = 18,
                        .damage = 85, .hitstun = 20, .blockstun = 16, .hitbox = {20, 80, 110, 30}, .kbx = 5, .cancelable = true};
    c.moves[M_CR_LIGHT] = {.name = "Etincelle basse", .input = "Bas + A", .limb = LIMB_LEG, .startup = 5, .active = 3, .recovery = 9,
                           .damage = 30, .hitstun = 13, .blockstun = 9, .hitbox = {20, 0, 70, 25}, .kbx = 2, .flags = MF_LOW, .cancelable = true};
    c.moves[M_CR_HEAVY] = {.name = "Arc rasant", .input = "Bas + B", .limb = LIMB_ARM, .startup = 11, .active = 5, .recovery = 22,
                           .damage = 75, .hitstun = 22, .blockstun = 16, .hitbox = {20, 0, 120, 25}, .kbx = 3, .kby = 6,
                           .flags = F(MF_LOW | MF_KNOCKDOWN)};
    c.moves[M_AIR_LIGHT] = {.name = "Pulse", .input = "Air + A", .limb = LIMB_ARM, .startup = 5, .active = 5, .recovery = 8,
                            .damage = 40, .hitstun = 15, .blockstun = 10, .hitbox = {10, 40, 60, 40}, .kbx = 2, .flags = MF_OVERHEAD};
    c.moves[M_AIR_HEAVY] = {.name = "Foudre plongeante", .input = "Air + B", .limb = LIMB_ARM, .startup = 9, .active = 6, .recovery = 12,
                            .damage = 85, .hitstun = 20, .blockstun = 14, .hitbox = {-10, -20, 80, 50}, .kbx = 3, .flags = MF_OVERHEAD};
    c.moves[M_SP_NEUTRAL] = {.name = "Orbe statique", .input = "C", .limb = LIMB_ARM, .startup = 14, .active = 1, .recovery = 26,
                             .proj = {.enabled = true, .speed = 4.5f, .w = 52, .h = 52, .yOff = 80, .life = 210,
                                      .damage = 35, .hitstun = 16, .blockstun = 12, .kbx = 1.5f, .hits = 3, .hitInterval = 10}};
    c.moves[M_SP_FORWARD] = {.name = "Lance electrique", .input = "Avant + C", .limb = LIMB_ARM, .startup = 16, .active = 4, .recovery = 26,
                             .damage = 100, .hitstun = 24, .blockstun = 16, .hitbox = {30, 85, 260, 25}, .kbx = 6};
    c.moves[M_SP_DOWN] = {.name = "Pilier de foudre", .input = "Bas + C", .limb = LIMB_ARM, .startup = 9, .active = 8, .recovery = 26,
                          .damage = 110, .hitstun = 28, .blockstun = 16, .hitbox = {30, 60, 80, 200}, .kbx = 2, .kby = 14,
                          .flags = F(MF_LAUNCH | MF_KNOCKDOWN), .invulnStart = 0, .invulnEnd = 7};
    c.moves[M_SUPER] = {.name = "Tempete", .input = "D (jauge pleine)", .limb = LIMB_ARM, .startup = 25, .active = 40, .recovery = 30,
                        .damage = 45, .hitstun = 22, .blockstun = 10, .hitbox = {0, 0, 500, 260}, .kbx = 1, .kby = 4,
                        .hits = 7, .hitInterval = 6, .invulnStart = 0, .invulnEnd = 26, .meterCost = SUPER_COST};
    return c;
}

static const std::vector<CharacterDef>& roster() {
    static const std::vector<CharacterDef> r = {makeKael(), makeBrakka(), makeNyx(), makeVolt()};
    return r;
}

int characterCount() { return static_cast<int>(roster().size()); }

const CharacterDef& getCharacter(int id) {
    const auto& r = roster();
    if (id < 0 || id >= static_cast<int>(r.size())) id = 0;
    return r[static_cast<size_t>(id)];
}

} // namespace fg
