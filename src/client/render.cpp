#include "client/render.h"

#include <algorithm>
#include <cmath>

using namespace fg;

namespace client {

Color toColor(uint32_t c) {
    return Color{static_cast<unsigned char>(c >> 24), static_cast<unsigned char>(c >> 16), static_cast<unsigned char>(c >> 8),
                 static_cast<unsigned char>(c)};
}

void drawTextCentered(const char* text, int cx, int y, int size, Color c) {
    DrawText(text, cx - MeasureText(text, size) / 2, y, size, c);
}

void drawTextOutlined(const char* text, int x, int y, int size, Color c, Color outline) {
    const int o = std::max(2, size / 14);
    for (int dx = -o; dx <= o; dx += o)
        for (int dy = -o; dy <= o; dy += o)
            if (dx || dy) DrawText(text, x + dx, y + dy, size, outline);
    DrawText(text, x, y, size, c);
}

static void bigCentered(const char* text, int y, int size, Color c) {
    drawTextOutlined(text, VW / 2 - MeasureText(text, size) / 2, y, size, c, Color{20, 10, 30, 255});
}

// ---------------------------------------------------------------------------
// Décor
// ---------------------------------------------------------------------------
void drawStageBackground(float camX, int frame) {
    DrawRectangleGradientV(0, 0, VW, static_cast<int>(GROUND_Y), Color{28, 18, 60, 255}, Color{236, 110, 80, 255});
    // lune
    DrawCircle(980 - static_cast<int>((camX - 800) * 0.05f), 150, 70, Color{255, 230, 200, 60});
    DrawCircle(980 - static_cast<int>((camX - 800) * 0.05f), 150, 52, Color{255, 236, 210, 255});
    // étoiles
    for (int i = 0; i < 40; ++i) {
        const int sx = (i * 137) % VW, sy = (i * 61) % 260;
        const float tw = 0.5f + 0.5f * std::sin(frame * 0.05f + i);
        DrawPixel(sx, sy, Fade(WHITE, 0.3f + 0.5f * tw));
    }
    // montagnes lointaines (parallaxe)
    const float p1 = -(camX - 800) * 0.15f;
    for (int i = -2; i < 8; ++i) {
        const float bx = p1 + i * 260.f;
        const float h = 160.f + static_cast<float>((i * 53 + 400) % 90);
        DrawTriangle({bx + 130, GROUND_Y - 120 - h}, {bx - 60, GROUND_Y - 100}, {bx + 320, GROUND_Y - 100}, Color{70, 40, 90, 255});
    }
    const float p2 = -(camX - 800) * 0.35f;
    for (int i = -2; i < 10; ++i) {
        const float bx = p2 + i * 190.f;
        const float h = 90.f + static_cast<float>((i * 37 + 300) % 70);
        DrawTriangle({bx + 95, GROUND_Y - 60 - h}, {bx - 40, GROUND_Y - 40}, {bx + 230, GROUND_Y - 40}, Color{48, 28, 66, 255});
    }
    // temple et lanternes (plan intermédiaire)
    const float p3 = -(camX - 800) * 0.6f;
    const float tx = 640 + p3;
    DrawRectangle(static_cast<int>(tx - 90), static_cast<int>(GROUND_Y - 170), 180, 130, Color{36, 20, 44, 255});
    DrawTriangle({tx, GROUND_Y - 250}, {tx - 150, GROUND_Y - 165}, {tx + 150, GROUND_Y - 165}, Color{36, 20, 44, 255});
    DrawTriangle({tx, GROUND_Y - 300}, {tx - 100, GROUND_Y - 235}, {tx + 100, GROUND_Y - 235}, Color{36, 20, 44, 255});
    for (int i = -3; i <= 3; ++i) {
        if (i == 0) continue;
        const float lx = tx + i * 230.f;
        DrawRectangle(static_cast<int>(lx - 3), static_cast<int>(GROUND_Y - 150), 6, 110, Color{30, 16, 36, 255});
        const float glow = 0.6f + 0.4f * std::sin(frame * 0.07f + i);
        DrawCircle(static_cast<int>(lx), static_cast<int>(GROUND_Y - 158), 22, Fade(Color{255, 170, 60, 255}, 0.25f * glow));
        DrawRectangle(static_cast<int>(lx - 9), static_cast<int>(GROUND_Y - 170), 18, 24, Color{255, 150, 50, 255});
    }
    // sol
    DrawRectangleGradientV(0, static_cast<int>(GROUND_Y - 40), VW, VH - static_cast<int>(GROUND_Y) + 40, Color{92, 52, 60, 255},
                           Color{40, 22, 34, 255});
    DrawRectangle(0, static_cast<int>(GROUND_Y - 40), VW, 3, Color{150, 90, 90, 255});
    const float off = -(camX - 800);
    for (int i = -12; i <= 12; ++i) {
        const float x0 = VW / 2.f + off * 0.6f + i * 70.f;
        const float x1 = VW / 2.f + off * 1.4f + i * 170.f;
        DrawLineEx({x0, GROUND_Y - 38}, {x1, static_cast<float>(VH)}, 2, Color{120, 70, 80, 120});
    }
}

// ---------------------------------------------------------------------------
// Personnages (squelette procédural)
// ---------------------------------------------------------------------------
namespace {

struct Pose {
    Vector2 head, shoulder, hip, handF, handB, footF, footB;
    float alpha = 1.f;
};

Vector2 lerpV(Vector2 a, Vector2 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

Vector2 clampReach(Vector2 from, Vector2 to, float maxLen) {
    const float dx = to.x - from.x, dy = to.y - from.y;
    const float d = std::sqrt(dx * dx + dy * dy);
    if (d <= maxLen || d < 0.001f) return to;
    return {from.x + dx / d * maxLen, from.y + dy / d * maxLen};
}

// articulation (coude/genou) entre a et b, segments de longueur len
Vector2 joint(Vector2 a, Vector2 b, float len, float bend) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float d = std::max(0.001f, std::sqrt(dx * dx + dy * dy));
    const float h = std::sqrt(std::max(0.f, len * len - d * d * 0.25f));
    return {(a.x + b.x) * 0.5f - dy / d * h * bend, (a.y + b.y) * 0.5f + dx / d * h * bend};
}

Pose computePose(const Fighter& f, const CharacterDef& c, int frame) {
    const float u = c.height / 150.f;
    Pose p;
    const float br = std::sin(frame * 0.08f) * 2.f * u;
    auto stand = [&](float breathe) {
        p.hip = {0, 70 * u + breathe * 0.5f};
        p.shoulder = {4 * u, 120 * u + breathe};
        p.head = {6 * u, 139 * u + breathe};
        p.handF = {30 * u, 112 * u + breathe};
        p.handB = {18 * u, 104 * u + breathe};
        p.footF = {16 * u, 0};
        p.footB = {-18 * u, 0};
    };
    auto crouch = [&]() {
        p.hip = {0, 40 * u};
        p.shoulder = {10 * u, 86 * u};
        p.head = {14 * u, 103 * u};
        p.handF = {34 * u, 82 * u};
        p.handB = {22 * u, 76 * u};
        p.footF = {24 * u, 0};
        p.footB = {-24 * u, 0};
    };
    auto air = [&]() {
        p.hip = {0, 72 * u};
        p.shoulder = {2 * u, 120 * u};
        p.head = {4 * u, 138 * u};
        p.handF = {26 * u, 120 * u};
        p.handB = {12 * u, 126 * u};
        const float ext = f.vy < 0 ? 14 * u : 0;
        p.footF = {16 * u, 34 * u - ext};
        p.footB = {-10 * u, 40 * u - ext};
    };
    auto lying = [&]() {
        p.hip = {-10 * u, 12 * u};
        p.shoulder = {-55 * u, 12 * u};
        p.head = {-74 * u, 14 * u};
        p.handF = {-40 * u, 3 * u};
        p.handB = {-70 * u, 2 * u};
        p.footF = {40 * u, 4 * u};
        p.footB = {36 * u, 14 * u};
    };
    auto hurt = [&](bool inAir) {
        p.hip = {-4 * u, 70 * u};
        p.shoulder = {-14 * u, 116 * u};
        p.head = {-20 * u, 132 * u};
        p.handF = {10 * u, 132 * u};
        p.handB = {-34 * u, 118 * u};
        p.footF = inAir ? Vector2{22 * u, 22 * u} : Vector2{16 * u, 0};
        p.footB = inAir ? Vector2{32 * u, 34 * u} : Vector2{-20 * u, 0};
    };
    const bool inAir = f.y > 0.5f;

    switch (f.state) {
    case FState::Idle:
        stand(br);
        break;
    case FState::Walk: {
        stand(0);
        const float ph = f.stateFrame * 0.22f;
        p.footF = {16 * u + std::sin(ph) * 14 * u, std::max(0.f, std::cos(ph)) * 7 * u};
        p.footB = {-18 * u - std::sin(ph) * 14 * u, std::max(0.f, -std::cos(ph)) * 7 * u};
        p.hip.y += std::fabs(std::sin(ph)) * 2 * u;
        break;
    }
    case FState::Crouch:
        crouch();
        break;
    case FState::Jump:
        air();
        break;
    case FState::Attack: {
        const MoveDef& mv = c.moves[f.move < 0 ? 0 : f.move];
        if (inAir) air();
        else if (isCrouchSlot(f.move)) crouch();
        else stand(0);
        float t;
        const int sf = f.stateFrame;
        if (sf < mv.startup) t = 0.3f * static_cast<float>(sf) / std::max(1, mv.startup);
        else if (sf < mv.startup + mv.active) t = 1.f;
        else t = 1.f - static_cast<float>(sf - mv.startup - mv.active) / std::max(1, mv.recovery);
        t = std::clamp(t, 0.f, 1.f);
        Vector2 target = mv.hitbox.w > 0 ? Vector2{mv.hitbox.x + mv.hitbox.w * 0.5f, mv.hitbox.y + mv.hitbox.h * 0.5f}
                                         : Vector2{60 * u, 100 * u};
        if (mv.proj.enabled) target = {58 * u, mv.proj.yOff};
        if (mv.flags & MF_TELEPORT) {
            p.alpha = sf < mv.startup ? 1.f - static_cast<float>(sf) / mv.startup : std::min(1.f, (sf - mv.startup) / 8.f);
            break;
        }
        if (mv.limb == LIMB_LEG) {
            p.footF = lerpV(p.footF, clampReach(p.hip, target, 78 * u), t);
            p.hip.x -= 6 * u * t;
            p.shoulder.x -= 10 * u * t;
            p.head.x -= 12 * u * t;
        } else if (mv.limb == LIMB_BODY) {
            p.shoulder.x += 18 * u * t;
            p.head.x += 22 * u * t;
            p.handF = lerpV(p.handF, {40 * u, 100 * u}, t);
            p.handB = lerpV(p.handB, {34 * u, 90 * u}, t);
        } else {
            p.shoulder.x += 8 * u * t;
            p.head.x += 6 * u * t;
            p.handF = lerpV(p.handF, clampReach(p.shoulder, target, 56 * u), t);
            if (mv.proj.enabled || f.move == M_SUPER) p.handB = lerpV(p.handB, clampReach(p.shoulder, target, 50 * u), t);
            if (f.move == M_SP_DOWN && mv.moveVy > 0) { // uppercut : poing vers le ciel
                p.handF = lerpV(p.handF, {22 * u, 175 * u}, t);
            }
        }
        break;
    }
    case FState::Hitstun:
        hurt(inAir || f.launched);
        break;
    case FState::Blockstun:
        if (f.crouching) crouch();
        else stand(0);
        p.handF = {p.shoulder.x + 22 * u, p.shoulder.y + 2 * u};
        p.handB = {p.shoulder.x + 20 * u, p.shoulder.y - 14 * u};
        p.shoulder.x -= 4 * u;
        break;
    case FState::Knockdown:
        lying();
        break;
    case FState::KO:
        if (inAir) hurt(true);
        else lying();
        break;
    case FState::Victory: {
        stand(0);
        const float b = std::fabs(std::sin(f.stateFrame * 0.1f)) * 6 * u;
        p.handF = {14 * u, 178 * u + b};
        p.handB = {-22 * u, 100 * u};
        p.head.y += b * 0.3f;
        break;
    }
    }
    return p;
}

void drawLimb(Vector2 a, Vector2 b, float len, float bend, float thick, Color col) {
    const Vector2 j = joint(a, b, len, bend);
    DrawLineEx(a, j, thick, col);
    DrawLineEx(j, b, thick * 0.9f, col);
    DrawCircleV(j, thick * 0.5f, col);
    DrawCircleV(b, thick * 0.55f, col);
}

void drawFighterAt(const Fighter& f, Vector2 feet, float scale, int frame, bool flash) {
    const CharacterDef& c = getCharacter(f.charId);
    const float u = c.height / 150.f;
    Pose p = computePose(f, c, frame);
    const float dir = static_cast<float>(f.facing);
    auto S = [&](Vector2 l) { return Vector2{feet.x + l.x * dir * scale, feet.y - l.y * scale}; };

    float alpha = p.alpha;
    if (flash) alpha *= (frame / 3) % 2 ? 0.45f : 0.9f;
    Color body = Fade(toColor(c.color), alpha);
    Color dark = Fade(toColor(c.color2), alpha);
    const Color acc = Fade(toColor(c.accent), alpha);
    if (f.state == FState::Hitstun && f.stateFrame < 4) body = dark = Fade(WHITE, alpha);

    const float th = c.thickness * scale;
    const float arm = 28 * u * scale, leg = 38 * u * scale;
    const Vector2 hip = S(p.hip), sh = S(p.shoulder), head = S(p.head);
    // sens du pliage : genoux vers l'avant, coudes vers le bas
    const float kb = dir > 0 ? 1.f : -1.f;

    // ombre
    DrawEllipse(static_cast<int>(feet.x), static_cast<int>(GROUND_Y + 4), 38 * u * scale, 7 * scale, Fade(BLACK, 0.35f * alpha));

    // écharpe de Nyx / bandeau de Kael (derrière)
    if (f.charId == 2 || f.charId == 0) {
        const Vector2 anchor = f.charId == 2 ? Vector2{sh.x, sh.y - 4 * scale} : Vector2{head.x, head.y - 4 * scale};
        const float len = f.charId == 2 ? 46.f : 26.f;
        Vector2 prev = anchor;
        for (int i = 1; i <= 5; ++i) {
            const float wave = std::sin(frame * 0.25f + i * 0.9f) * 4.f * scale;
            Vector2 pt{anchor.x - dir * len * scale * i / 5.f, anchor.y + wave + i * 2.f * scale};
            DrawLineEx(prev, pt, (f.charId == 2 ? 9.f : 5.f) * scale * (1.f - i * 0.12f), acc);
            prev = pt;
        }
    }

    // membres arrière
    drawLimb(hip, S(p.footB), leg, kb * -1.f, th, dark);
    drawLimb(sh, S(p.handB), arm, kb * 1.f, th * 0.9f, dark);

    // torse
    const float torso = th * (f.charId == 1 ? 3.4f : 2.3f);
    DrawLineEx(hip, sh, torso, body);
    DrawCircleV(sh, torso * 0.5f, body);
    DrawCircleV(hip, torso * 0.42f, body);
    if (f.charId == 0) DrawLineEx({hip.x - 10 * scale, hip.y - 2 * scale}, {hip.x + 10 * scale, hip.y - 2 * scale}, 6 * scale, Fade(BLACK, alpha)); // ceinture
    if (f.charId == 1) DrawCircleV(sh, torso * 0.62f, dark); // épaulières

    // tête
    const float hr = (f.charId == 1 ? 12.f : 13.f) * u * scale;
    DrawCircleV(head, hr, f.charId == 2 ? dark : body);
    if (f.charId == 0) DrawLineEx({head.x - hr, head.y - hr * 0.35f}, {head.x + hr, head.y - hr * 0.35f}, 5 * scale, acc);
    if (f.charId == 3) DrawLineEx({head.x, head.y - 2 * scale}, {head.x + dir * hr, head.y - 2 * scale}, 6 * scale, acc);
    if (f.charId == 2) DrawLineEx({head.x - dir * hr * 0.1f, head.y - 2 * scale}, {head.x + dir * hr * 0.95f, head.y - 2 * scale}, 3 * scale, acc);
    if (f.state != FState::KO) DrawCircleV({head.x + dir * hr * 0.55f, head.y - hr * 0.15f}, 2.2f * scale, Fade(WHITE, alpha));

    // membres avant
    drawLimb(hip, S(p.footF), leg, kb * -1.f, th, body);
    drawLimb(sh, S(p.handF), arm, kb * 1.f, th * 0.95f, body);

    // étincelles de Volt
    if (f.charId == 3 && (frame / 4) % 2 == 0) {
        const Vector2 h = S(p.handF);
        for (int i = 0; i < 3; ++i) {
            const float a = (frame * 37 + i * 120) % 360 * DEG2RAD;
            DrawLineEx(h, {h.x + std::cos(a) * 12 * scale, h.y + std::sin(a) * 12 * scale}, 2, acc);
        }
    }
}

Vector2 toScreen(float wx, float wy, float camX, Vector2 shake) {
    return {wx - camX + VW / 2.f + shake.x, GROUND_Y - wy + shake.y};
}

void drawMoveEffect(const Fighter& f, float camX, Vector2 shake, bool hitboxes) {
    if (f.state != FState::Attack || f.move < 0) return;
    const CharacterDef& c = getCharacter(f.charId);
    const MoveDef& mv = c.moves[f.move];
    if (mv.hitbox.w <= 0) return;
    const bool active = moveIsActive(f, mv);
    const Box b = moveHitboxWorld(f, mv);
    const Vector2 tl = toScreen(b.x, b.y + b.h, camX, shake);
    const Rectangle r{tl.x, tl.y, b.w, b.h};
    if (active && (f.move >= M_SP_NEUTRAL || b.w > 100)) {
        const Color acc = toColor(c.accent);
        DrawRectangleRounded(r, 0.6f, 8, Fade(acc, 0.28f));
        if (f.charId == 3) { // éclairs
            Vector2 prev{r.x, r.y + r.height * 0.5f};
            for (int i = 1; i <= 8; ++i) {
                Vector2 pt{r.x + r.width * i / 8.f, r.y + static_cast<float>(GetRandomValue(0, static_cast<int>(r.height)))};
                DrawLineEx(prev, pt, 3, acc);
                prev = pt;
            }
        }
    }
    if (hitboxes) DrawRectangleLinesEx(r, 2, active ? RED : Fade(RED, 0.3f));
}

void drawProjectile(const Projectile& p, float camX, Vector2 shake, int frame) {
    const CharacterDef& c = getCharacter(p.charId);
    const Color acc = toColor(c.accent);
    const Vector2 center = toScreen(p.x + p.w * 0.5f, p.y + p.h * 0.5f, camX, shake);
    const float r = std::min(p.w, p.h) * 0.5f;
    switch (p.charId) {
    case 0:
        if (p.slot == M_SUPER) {
            const Vector2 tl = toScreen(p.x, p.y + p.h, camX, shake);
            DrawRectangleRounded({tl.x - 20, tl.y - 10, p.w + 40, p.h + 20}, 1.f, 12, Fade(acc, 0.35f));
            DrawRectangleRounded({tl.x, tl.y, p.w, p.h}, 1.f, 12, Fade(Color{255, 200, 120, 255}, 0.85f));
            DrawRectangleRounded({tl.x + 10, tl.y + p.h * 0.3f, p.w - 20, p.h * 0.4f}, 1.f, 12, WHITE);
        } else {
            DrawCircleV(center, r * 1.6f, Fade(acc, 0.3f));
            DrawCircleV(center, r, Color{255, 170, 90, 255});
            DrawCircleV(center, r * 0.55f, WHITE);
            for (int i = 1; i <= 3; ++i)
                DrawCircleV({center.x - (p.vx > 0 ? 1 : -1) * i * 14.f, center.y}, r * (0.7f - i * 0.15f), Fade(acc, 0.4f));
        }
        break;
    case 2: {
        const float rot = p.age * 25.f;
        DrawPoly(center, 4, r * 1.3f, rot, Color{200, 200, 220, 255});
        DrawPoly(center, 4, r * 1.3f, rot + 45, Color{120, 120, 150, 255});
        DrawCircleV(center, r * 0.35f, acc);
        break;
    }
    case 3: {
        DrawCircleV(center, r * 1.4f, Fade(acc, 0.2f + 0.1f * std::sin(frame * 0.5f)));
        DrawCircleV(center, r, Fade(toColor(c.color), 0.8f));
        DrawCircleV(center, r * 0.5f, WHITE);
        for (int i = 0; i < 5; ++i) {
            const float a = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
            DrawLineEx(center, {center.x + std::cos(a) * r * 1.5f, center.y + std::sin(a) * r * 1.5f}, 2, acc);
        }
        break;
    }
    default:
        DrawCircleV(center, r, acc);
        break;
    }
}

void drawSpark(const Spark& s, float camX, Vector2 shake) {
    if (s.age > 20) return;
    const Vector2 c = toScreen(s.x, s.y, camX, shake);
    const float t = s.age / 20.f;
    switch (s.type) {
    case 0:
    case 2: {
        const float big = s.type == 2 ? 1.6f : 1.f;
        DrawCircleV(c, (10 + 40 * t) * big, Fade(Color{255, 240, 150, 255}, 0.5f * (1 - t)));
        DrawRing(c, (16 + 50 * t) * big, (20 + 54 * t) * big, 0, 360, 24, Fade(WHITE, 1 - t));
        for (int i = 0; i < 8; ++i) {
            const float a = (i * 45 + s.seq * 17) * DEG2RAD;
            const float r0 = 10 * big + 30 * t * big, r1 = r0 + 22 * big * (1 - t);
            DrawLineEx({c.x + std::cos(a) * r0, c.y + std::sin(a) * r0}, {c.x + std::cos(a) * r1, c.y + std::sin(a) * r1}, 4,
                       Fade(Color{255, 200, 80, 255}, 1 - t));
        }
        break;
    }
    case 1:
        DrawRing(c, 24 + 10 * t, 30 + 12 * t, 0, 360, 24, Fade(Color{120, 200, 255, 255}, 1 - t));
        DrawCircleV(c, 26 + 10 * t, Fade(Color{120, 200, 255, 255}, 0.25f * (1 - t)));
        break;
    case 3:
        for (int i = 0; i < 6; ++i) {
            const float a = (i * 60 + s.seq * 31) * DEG2RAD;
            DrawCircleV({c.x + std::cos(a) * 30 * t, c.y + std::sin(a) * 20 * t}, 10 * (1 - t), Fade(LIGHTGRAY, 0.7f * (1 - t)));
        }
        break;
    case 4:
        DrawRing(c, 40 + 300 * t, 50 + 320 * t, 0, 360, 48, Fade(WHITE, 1 - t));
        break;
    default:
        break;
    }
}

void drawHealthBar(const Fighter& f, float shown, int side, const std::string& name, int wins, int frame) {
    const CharacterDef& c = getCharacter(f.charId);
    const int x = side == 0 ? 40 : VW - 40 - 520, y = 28, w = 520, h = 26;
    const float ratio = std::clamp(static_cast<float>(f.health) / c.maxHealth, 0.f, 1.f);
    const float shownR = std::clamp(shown / c.maxHealth, 0.f, 1.f);
    DrawRectangle(x - 4, y - 4, w + 8, h + 8, Color{20, 10, 30, 230});
    DrawRectangle(x, y, w, h, Color{60, 20, 30, 255});
    auto fill = [&](float r, Color top, Color bot) {
        const int fw = static_cast<int>(w * r);
        const int fx = side == 0 ? x + (w - fw) : x;
        DrawRectangleGradientV(fx, y, fw, h, top, bot);
    };
    fill(shownR, Color{255, 80, 60, 255}, Color{180, 30, 30, 255});
    const Color hpTop = ratio > 0.3f ? Color{255, 230, 90, 255} : Color{255, 120, 60, 255};
    fill(ratio, hpTop, ratio > 0.3f ? Color{230, 160, 30, 255} : Color{200, 50, 30, 255});
    if (ratio <= 0.3f && (frame / 10) % 2) DrawRectangleLinesEx({float(x), float(y), float(w), float(h)}, 2, RED);
    const char* label = TextFormat("%s  %s", c.name, name.c_str());
    if (side == 0) drawTextOutlined(label, x, y + h + 8, 22, WHITE, BLACK);
    else drawTextOutlined(label, x + w - MeasureText(label, 22), y + h + 8, 22, WHITE, BLACK);
    for (int i = 0; i < ROUNDS_TO_WIN; ++i) {
        const int cx = side == 0 ? x + w - 14 - i * 30 : x + 14 + i * 30;
        DrawCircle(cx, y + h + 20, 10, Color{20, 10, 30, 255});
        DrawCircle(cx, y + h + 20, 7, i < wins ? Color{255, 210, 60, 255} : Color{70, 50, 70, 255});
    }
}

void drawMeter(const Fighter& f, int side, int frame) {
    const int w = 320, h = 16, y = VH - 38;
    const int x = side == 0 ? 40 : VW - 40 - w;
    const float r = std::clamp(static_cast<float>(f.meter) / METER_MAX, 0.f, 1.f);
    DrawRectangle(x - 3, y - 3, w + 6, h + 6, Color{20, 10, 30, 230});
    DrawRectangle(x, y, w, h, Color{30, 30, 60, 255});
    const int fw = static_cast<int>(w * r);
    const bool full = f.meter >= SUPER_COST;
    const Color col = full ? ((frame / 6) % 2 ? Color{120, 255, 255, 255} : Color{60, 160, 255, 255}) : Color{60, 140, 255, 255};
    DrawRectangle(side == 0 ? x : x + w - fw, y, fw, h, col);
    const char* t = full ? "SUPER PRET ! [D]" : "SUPER";
    if (side == 0) drawTextOutlined(t, x, y - 26, 20, full ? YELLOW : LIGHTGRAY, BLACK);
    else drawTextOutlined(t, x + w - MeasureText(t, 20), y - 26, 20, full ? YELLOW : LIGHTGRAY, BLACK);
}

} // namespace

void drawFighterPreview(int charId, Vector2 feet, float scale, int frame, int facing) {
    Fighter f;
    f.charId = charId;
    f.facing = facing;
    f.state = FState::Idle;
    f.stateFrame = frame;
    drawFighterAt(f, feet, scale, frame, false);
}

void drawMatch(const Match& m, const RenderOpts& o, HudMemory& hud) {
    const int frame = static_cast<int>(m.frame);
    // caméra
    const float mid = (m.f[0].x + m.f[1].x) * 0.5f;
    const float target = std::clamp(mid, VW / 2.f, STAGE_RIGHT - VW / 2.f);
    hud.camX += (target - hud.camX) * 0.2f;
    if (std::fabs(target - hud.camX) > 300) hud.camX = target;
    Vector2 shake{0, 0};
    if (m.hitstop > 0) {
        const float a = std::min(8.f, m.hitstop * 0.8f);
        shake = {static_cast<float>(GetRandomValue(-100, 100)) / 100.f * a, static_cast<float>(GetRandomValue(-100, 100)) / 100.f * a};
    }
    drawStageBackground(hud.camX, frame);

    // super : assombrissement
    for (const auto& f : m.f)
        if (f.state == FState::Attack && f.move == M_SUPER && f.stateFrame < 30) DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.45f));

    // persos (celui qui attaque devant)
    int order[2] = {0, 1};
    if (m.f[0].state == FState::Attack && m.f[1].state != FState::Attack) std::swap(order[0], order[1]);
    for (int k = 0; k < 2; ++k) {
        const Fighter& f = m.f[order[k]];
        const Vector2 feet = toScreen(f.x, f.y, hud.camX, shake);
        drawFighterAt(f, feet, 1.f, frame, f.invuln > 0 && f.state != FState::KO);
        if (o.hitboxes) {
            const Box hb = hurtbox(f);
            const Vector2 tl = toScreen(hb.x, hb.y + hb.h, hud.camX, shake);
            DrawRectangleLinesEx({tl.x, tl.y, hb.w, hb.h}, 2, isInvulnerable(f) ? Fade(WHITE, 0.6f) : Fade(GREEN, 0.8f));
        }
        if (o.localSlot == order[k]) {
            const Vector2 top = toScreen(f.x, f.y + getCharacter(f.charId).height + 40, hud.camX, shake);
            drawTextCentered("TOI", static_cast<int>(top.x), static_cast<int>(top.y), 20, YELLOW);
            DrawTriangle({top.x, top.y + 32}, {top.x - 8, top.y + 22}, {top.x + 8, top.y + 22}, YELLOW);
        }
    }
    for (int i = 0; i < 2; ++i) drawMoveEffect(m.f[i], hud.camX, shake, o.hitboxes);
    for (const auto& p : m.proj)
        if (p.active) {
            drawProjectile(p, hud.camX, shake, frame);
            if (o.hitboxes) {
                const Vector2 tl = toScreen(p.x, p.y + p.h, hud.camX, shake);
                DrawRectangleLinesEx({tl.x, tl.y, p.w, p.h}, 2, ORANGE);
            }
        }
    for (const auto& s : m.sparks) drawSpark(s, hud.camX, shake);

    // HUD
    for (int i = 0; i < 2; ++i) {
        const int target = m.f[i].health;
        if (hud.shownHealth[i] < 0 || hud.shownHealth[i] < target) hud.shownHealth[i] = static_cast<float>(target);
        else if (m.f[i].state != FState::Hitstun || m.f[i].combo == 0) hud.shownHealth[i] += (target - hud.shownHealth[i]) * 0.06f;
        drawHealthBar(m.f[i], hud.shownHealth[i], i, o.names[i], m.wins[i], frame);
        drawMeter(m.f[i], i, frame);
    }
    // chrono
    DrawRectangle(VW / 2 - 44, 14, 88, 62, Color{20, 10, 30, 230});
    const char* tm = m.training ? "--" : TextFormat("%02d", std::max(0, (m.roundTime + FPS - 1) / FPS));
    drawTextCentered(tm, VW / 2, 22, 46, m.roundTime < 10 * FPS && !m.training ? RED : WHITE);

    // combos : affichés du côté de l'attaquant
    for (int d = 0; d < 2; ++d) {
        const int atk = 1 - d;
        if (m.f[d].combo >= 2) {
            hud.comboShow[atk] = m.f[d].combo;
            hud.comboDamage[atk] = m.f[d].comboDamage;
            hud.comboTimer[atk] = 70;
        } else if (hud.comboTimer[atk] > 0) {
            hud.comboTimer[atk]--;
        }
        if (hud.comboTimer[atk] > 0 && hud.comboShow[atk] >= 2) {
            const int x = atk == 0 ? 50 : VW - 50 - 220;
            const Color col = Fade(Color{255, 220, 80, 255}, std::min(1.f, hud.comboTimer[atk] / 20.f));
            drawTextOutlined(TextFormat("%d COUPS", hud.comboShow[atk]), x, 170, 44, col, BLACK);
            drawTextOutlined(TextFormat("%d degats", hud.comboDamage[atk]), x, 216, 22, Fade(WHITE, col.a / 255.f), BLACK);
        }
    }

    // annonces
    switch (m.phase) {
    case Phase::Intro:
        if (m.phaseTimer > 45) bigCentered(TextFormat("MANCHE %d", m.round), 250, 90, WHITE);
        else bigCentered("COMBATTEZ !", 250, 100, Color{255, 200, 60, 255});
        break;
    case Phase::RoundEnd: {
        if (m.phaseTimer > 120) {
            bigCentered(m.timeOut ? "TEMPS ECOULE" : (m.roundWinner == 2 ? "DOUBLE K.O." : "K.O."), 240, 120,
                        Color{255, 60, 50, 255});
        } else {
            const char* t = m.roundWinner == 2 ? "EGALITE"
                                               : TextFormat("%s GAGNE LA MANCHE", getCharacter(m.f[m.roundWinner].charId).name);
            bigCentered(t, 260, 60, WHITE);
        }
        break;
    }
    case Phase::MatchEnd: {
        const int w = m.wins[0] >= ROUNDS_TO_WIN ? 0 : 1;
        const std::string who = o.names[w].empty() ? getCharacter(m.f[w].charId).name : o.names[w];
        bigCentered(TextFormat("%s REMPORTE LE MATCH !", who.c_str()), 230, 64, Color{255, 210, 60, 255});
        break;
    }
    case Phase::Waiting:
        DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.5f));
        bigCentered("EN ATTENTE D'UN ADVERSAIRE...", 300, 50, WHITE);
        break;
    default:
        break;
    }
    if (!o.bottomHint.empty()) drawTextCentered(o.bottomHint.c_str(), VW / 2, VH - 30, 18, Fade(WHITE, 0.8f));
}

} // namespace client
