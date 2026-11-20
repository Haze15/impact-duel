#include "client/ai.h"
#include <cmath>

using namespace fg;

namespace client {

static float rnd(AIState& s) {
    s.rng ^= s.rng << 13;
    s.rng ^= s.rng >> 17;
    s.rng ^= s.rng << 5;
    return static_cast<float>(s.rng & 0xFFFFFF) / static_cast<float>(0x1000000);
}

Input aiInput(const Match& m, int me, AIState& s) {
    Input out;
    if (m.phase != Phase::Fight) return out;
    const Fighter& f = m.f[me];
    const Fighter& o = m.f[1 - me];
    const CharacterDef& c = getCharacter(f.charId);
    const uint8_t fwd = f.facing > 0 ? IN_RIGHT : IN_LEFT;
    const uint8_t back = f.facing > 0 ? IN_LEFT : IN_RIGHT;
    const float dist = std::fabs(o.x - f.x);
    const bool oAir = o.y > 0.f;
    const float react = s.level == 1 ? 0.25f : (s.level == 2 ? 0.55f : 0.85f);

    // --- défense réactive
    bool threat = false;
    uint16_t threatFlags = 0;
    if (o.state == FState::Attack && o.move >= 0) {
        const MoveDef& mv = getCharacter(o.charId).moves[o.move];
        const float reach = mv.hitbox.x + mv.hitbox.w + 60.f;
        if (mv.hitbox.w > 0 && dist < reach && o.stateFrame <= mv.startup + mv.active && !(mv.flags & MF_GRAB)) {
            threat = true;
            threatFlags = mv.flags;
            if (o.y > 0) threatFlags |= MF_OVERHEAD;
        }
    }
    for (const auto& p : m.proj) {
        if (!p.active || p.owner == me) continue;
        const float px = p.x + p.w * 0.5f;
        const bool coming = (p.vx > 0 && px < f.x) || (p.vx < 0 && px > f.x);
        if (coming && std::fabs(px - f.x) < 260.f) threat = true;
    }
    const bool canAct = f.state == FState::Idle || f.state == FState::Walk || f.state == FState::Crouch ||
                        f.state == FState::Blockstun;
    if (threat && canAct) {
        if (s.think <= 0 || (s.plan & back) == 0) {
            if (rnd(s) < react) {
                s.plan = back;
                if (threatFlags & MF_LOW) s.plan |= IN_DOWN;
                s.think = 10;
            } else {
                s.think = 6;
            }
        }
        out.held = s.plan;
        if (--s.think < 0) s.think = 0;
        return out;
    }

    // annulation sur touche (hit-confirm)
    if (f.state == FState::Attack && f.move >= 0 && f.hitsDone > 0 && c.moves[f.move].cancelable && rnd(s) < react * 0.5f) {
        out.pressed = (f.meter >= SUPER_COST && rnd(s) < 0.5f) ? IN_D : IN_C;
        out.held = static_cast<uint8_t>(rnd(s) < 0.5f ? fwd : (rnd(s) < 0.5f ? IN_DOWN : 0));
        return out;
    }

    if (s.pressDelay > 0 && --s.pressDelay == 0) {
        out.pressed = s.queuedPress;
        s.queuedPress = 0;
    }

    if (--s.think <= 0) {
        s.think = 8 + static_cast<int>(rnd(s) * (s.level == 3 ? 10 : 22));
        s.plan = 0;
        const float r = rnd(s);
        const bool hasProj = c.moves[M_SP_NEUTRAL].proj.enabled;
        auto press = [&](uint8_t b, uint8_t dirs, int delay = 1) {
            s.plan = dirs;
            s.queuedPress = b;
            s.pressDelay = delay;
        };
        if (f.y > 0) {
            if (dist < 150) press(r < 0.5f ? IN_B : IN_A, 0);
        } else if (f.meter >= SUPER_COST && dist < 260 && r < 0.6f) {
            press(IN_D, 0);
        } else if (oAir && dist < 190 && r < react) {
            press(IN_C, IN_DOWN); // anti-air
        } else if (dist > 450) {
            if (hasProj && r < 0.35f) press(IN_C, 0);
            else if (r < 0.8f) s.plan = fwd;
            else s.plan = static_cast<uint8_t>(IN_UP | fwd);
        } else if (dist > 170) {
            if (r < 0.40f) s.plan = fwd;
            else if (r < 0.55f) press(IN_C, fwd);
            else if (r < 0.70f && hasProj) press(IN_C, 0);
            else if (r < 0.82f) { s.plan = static_cast<uint8_t>(IN_UP | fwd); s.think = 14; }
            else s.plan = back;
        } else {
            if (r < 0.28f) press(IN_A, 0);
            else if (r < 0.46f) press(IN_B, 0);
            else if (r < 0.60f) press(IN_B, IN_DOWN);
            else if (r < 0.68f) press(IN_A, IN_DOWN);
            else if (r < 0.78f) press(IN_C, 0);
            else if (r < 0.86f) press(IN_C, IN_DOWN);
            else s.plan = static_cast<uint8_t>(back | (rnd(s) < 0.5f ? IN_DOWN : 0));
        }
    }
    out.held = static_cast<uint8_t>(s.plan | out.pressed);
    if (s.plan & IN_UP) s.plan &= static_cast<uint8_t>(~IN_UP); // un seul saut
    return out;
}

} // namespace client
