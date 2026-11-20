#pragma once
#include <cstdint>

namespace fg {

// Bits d'entrée (directions absolues + 4 boutons)
enum InputBit : uint8_t {
    IN_UP    = 1 << 0,
    IN_DOWN  = 1 << 1,
    IN_LEFT  = 1 << 2,
    IN_RIGHT = 1 << 3,
    IN_A     = 1 << 4, // coup léger
    IN_B     = 1 << 5, // coup lourd
    IN_C     = 1 << 6, // spécial
    IN_D     = 1 << 7, // super
};
constexpr uint8_t IN_DIRS = IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT;
constexpr uint8_t IN_BUTTONS = IN_A | IN_B | IN_C | IN_D;

struct Input {
    uint8_t held = 0;    // touches maintenues
    uint8_t pressed = 0; // touches appuyées cette frame
};

// Boîte : x,y = coin bas-gauche, y vers le haut (monde)
struct Box {
    float x = 0, y = 0, w = 0, h = 0;
};
inline bool overlap(const Box& a, const Box& b) {
    return a.w > 0 && b.w > 0 && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

constexpr int FPS = 60;
constexpr float STAGE_LEFT = 0.f;
constexpr float STAGE_RIGHT = 1600.f;
constexpr float MAX_SEPARATION = 1050.f; // les deux persos restent à l'écran
constexpr float GRAVITY = 0.9f;
constexpr int SUPER_COST = 1000;
constexpr int METER_MAX = 1000;
constexpr int ROUND_TIME = 99 * FPS;
constexpr int ROUNDS_TO_WIN = 2;

} // namespace fg
