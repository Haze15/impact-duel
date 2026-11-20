#pragma once
#include "core/sim.h"
#include "raylib.h"
#include <string>

namespace client {

constexpr int VW = 1280; // résolution virtuelle
constexpr int VH = 720;
constexpr float GROUND_Y = 640.f;

struct HudMemory {
    float shownHealth[2] = {-1, -1};
    int comboShow[2] = {0, 0};
    int comboDamage[2] = {0, 0};
    int comboTimer[2] = {0, 0};
    float camX = 800.f;
};

struct RenderOpts {
    bool hitboxes = false;
    int localSlot = -1;          // en ligne : indique « TOI » au-dessus du perso
    std::string names[2];
    std::string bottomHint;
};

Color toColor(uint32_t rgba);
void drawMatch(const fg::Match& m, const RenderOpts& o, HudMemory& hud);
void drawStageBackground(float camX, int frame);
void drawFighterPreview(int charId, Vector2 feet, float scale, int frame, int facing);
void drawTextCentered(const char* text, int cx, int y, int size, Color c);
void drawTextOutlined(const char* text, int x, int y, int size, Color c, Color outline);

} // namespace client
