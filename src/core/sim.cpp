#include "core/sim.h"
#include <algorithm>
#include <cmath>

namespace fg {

namespace {

struct HitInfo {
    int damage, hitstun, blockstun;
    float kbx, kby;
    uint16_t flags;
    bool special;
    bool super;
};

bool grounded(const Fighter& f) { return f.y <= 0.f; }

void setState(Fighter& f, FState s) {
    if (f.state != s) {
        f.state = s;
        f.stateFrame = 0;
    }
}

const MoveDef& curMove(const Fighter& f) { return getCharacter(f.charId).moves[f.move < 0 ? 0 : f.move]; }

void addSpark(Match& m, float x, float y, int type) {
    Spark& s = m.sparks[m.sparkSeq % MAX_SPARKS];
    s.x = x;
    s.y = y;
    s.type = type;
    s.age = 0;
    s.seq = ++m.sparkSeq;
}

bool hasProjectile(const Match& m, int owner) {
    for (const auto& p : m.proj)
        if (p.active && p.owner == owner) return true;
    return false;
}

void startMove(Fighter& f, int slot) {
    const MoveDef& mv = getCharacter(f.charId).moves[slot];
    f.state = FState::Attack;
    f.stateFrame = 0;
    f.move = slot;
    f.hitsDone = 0;
    f.lastHitFrame = -100;
    f.meter -= mv.meterCost;
    f.buffer = 0;
    f.bufferTimer = 0;
    if (grounded(f) && mv.moveStart < 0) f.vx = 0;
}

bool tryAttack(Match& m, int i, uint8_t pressed, bool down, bool fwd, bool specialsOnly) {
    Fighter& f = m.f[i];
    const CharacterDef& c = getCharacter(f.charId);
    const bool g = grounded(f);
    int slot = -1;
    if ((pressed & IN_D) && f.meter >= SUPER_COST)
        slot = M_SUPER;
    else if (pressed & IN_C)
        slot = down ? M_SP_DOWN : (fwd ? M_SP_FORWARD : M_SP_NEUTRAL);
    else if (!specialsOnly && (pressed & IN_A))
        slot = g ? (down ? M_CR_LIGHT : M_LIGHT) : M_AIR_LIGHT;
    else if (!specialsOnly && (pressed & IN_B))
        slot = g ? (down ? M_CR_HEAVY : M_HEAVY) : M_AIR_HEAVY;
    if (slot < 0) return false;
    const MoveDef& mv = c.moves[slot];
    if (!g && isSpecialSlot(slot) && !(mv.flags & MF_AIR_OK)) {
        // un spécial au sol en l'air : on retente la version neutre si elle est aérienne
        if (slot != M_SUPER && (c.moves[M_SP_NEUTRAL].flags & MF_AIR_OK) && !hasProjectile(m, i))
            slot = M_SP_NEUTRAL;
        else
            return false;
    }
    if (c.moves[slot].proj.enabled && slot != M_SUPER && hasProjectile(m, i)) return false;
    startMove(f, slot);
    return true;
}

void handleInput(Match& m, int i, const Input& in) {
    Fighter& f = m.f[i];
    const CharacterDef& c = getCharacter(f.charId);
    f.held = in.held;
    if (in.pressed) {
        f.buffer |= in.pressed;
        f.bufferTimer = 8;
    }
    const uint8_t pressed = f.buffer;
    const uint8_t fwdBit = f.facing > 0 ? IN_RIGHT : IN_LEFT;
    const uint8_t backBit = f.facing > 0 ? IN_LEFT : IN_RIGHT;
    const bool up = in.held & IN_UP, down = in.held & IN_DOWN;
    const bool fwd = in.held & fwdBit, back = in.held & backBit;

    switch (f.state) {
    case FState::Idle:
    case FState::Walk:
    case FState::Crouch:
        f.crouching = down;
        if (tryAttack(m, i, pressed, down, fwd, false)) return;
        if (up) {
            setState(f, FState::Jump);
            f.vy = c.jumpVel;
            f.vx = fwd ? c.jumpVx * f.facing : (back ? -c.jumpVx * f.facing : 0.f);
            f.airJumps = c.airJumps;
            f.helpless = false;
            f.crouching = false;
            f.buffer &= static_cast<uint8_t>(~IN_UP);
        } else if (down) {
            setState(f, FState::Crouch);
            f.vx = 0;
        } else if (fwd) {
            setState(f, FState::Walk);
            f.vx = c.walkSpeed * f.facing;
        } else if (back) {
            setState(f, FState::Walk);
            f.vx = -c.backSpeed * f.facing;
        } else {
            setState(f, FState::Idle);
            f.vx = 0;
        }
        break;
    case FState::Jump:
        if (f.helpless) break;
        if (tryAttack(m, i, pressed, down, fwd, false)) return;
        if ((pressed & IN_UP) && f.airJumps > 0 && f.stateFrame > 4) {
            f.airJumps--;
            f.vy = c.jumpVel * 0.85f;
            f.vx = fwd ? c.jumpVx * f.facing : (back ? -c.jumpVx * f.facing : f.vx * 0.5f);
            f.stateFrame = 0;
            f.buffer &= static_cast<uint8_t>(~IN_UP);
            if (m.f[1 - i].x != f.x) f.facing = m.f[1 - i].x > f.x ? 1 : -1;
        }
        break;
    case FState::Attack: {
        const MoveDef& mv = curMove(f);
        // annulation en spécial / super quand le coup a touché ou été paré
        if (f.hitsDone > 0 && mv.cancelable && (pressed & (IN_C | IN_D))) tryAttack(m, i, pressed, down, fwd, true);
        break;
    }
    default:
        break;
    }
}

void spawnProjectile(Match& m, int i, int slot) {
    const Fighter& f = m.f[i];
    const CharacterDef& c = getCharacter(f.charId);
    const ProjectileDef& pd = c.moves[slot].proj;
    for (auto& p : m.proj) {
        if (p.active) continue;
        p.active = true;
        p.owner = i;
        p.charId = f.charId;
        p.slot = slot;
        p.w = pd.w;
        p.h = pd.h;
        p.x = f.x + f.facing * (c.width * 0.5f + pd.w * 0.5f) - pd.w * 0.5f;
        p.y = f.y + pd.yOff - pd.h * 0.5f;
        p.vx = pd.speed * f.facing;
        p.vy = grounded(f) ? 0.f : pd.airVy;
        p.life = pd.life;
        p.hitsLeft = pd.hits;
        p.cooldown = 0;
        p.age = 0;
        return;
    }
}

void applyMoveEffects(Match& m, int i) {
    Fighter& f = m.f[i];
    if (f.state != FState::Attack) return;
    const MoveDef& mv = curMove(f);
    const int sf = f.stateFrame;
    if (mv.moveStart >= 0) {
        if (sf == mv.moveStart && mv.moveVy != 0) f.vy = mv.moveVy;
        if (sf >= mv.moveStart && sf < mv.moveEnd) f.vx = mv.moveVx * f.facing;
        else if (sf == mv.moveEnd && grounded(f)) f.vx = 0;
    }
    if (sf == mv.startup) {
        if (mv.proj.enabled) spawnProjectile(m, i, f.move);
        if (mv.flags & MF_TELEPORT) {
            const Fighter& o = m.f[1 - i];
            const float side = (o.x >= f.x) ? 1.f : -1.f;
            f.x = std::clamp(o.x + side * 90.f, STAGE_LEFT + 40.f, STAGE_RIGHT - 40.f);
            f.facing = (o.x >= f.x) ? 1 : -1;
            addSpark(m, f.x, f.y + 70, 3);
        }
    }
}

void physics(Match& m) {
    for (int i = 0; i < 2; ++i) {
        Fighter& f = m.f[i];
        const CharacterDef& c = getCharacter(f.charId);
        const bool wasAir = !grounded(f);
        if (wasAir || f.vy > 0) f.vy -= GRAVITY;
        if (grounded(f) && (f.state == FState::Hitstun || f.state == FState::Blockstun || f.state == FState::KO ||
                            f.state == FState::Knockdown))
            f.vx *= 0.86f;
        f.x += f.vx;
        f.y += f.vy;
        if (f.y <= 0.f) {
            f.y = 0.f;
            if (wasAir) {
                f.vy = 0;
                f.helpless = false;
                switch (f.state) {
                case FState::Jump:
                    setState(f, FState::Idle);
                    f.vx = 0;
                    break;
                case FState::Attack:
                    if (isAirSlot(f.move) || f.stateFrame >= curMove(f).startup + curMove(f).active ||
                        (curMove(f).flags & MF_AIR_OK)) {
                        setState(f, FState::Idle);
                        f.move = -1;
                        f.vx = 0;
                    }
                    break;
                case FState::Hitstun:
                    if (f.knockFlag) {
                        setState(f, FState::Knockdown);
                        addSpark(m, f.x, 5, 3);
                    } else {
                        setState(f, FState::Idle);
                        f.combo = 0;
                    }
                    f.launched = false;
                    f.knockFlag = false;
                    f.vx *= 0.3f;
                    break;
                case FState::KO:
                    f.vx *= 0.4f;
                    break;
                default:
                    break;
                }
            }
        }
        const float hw = c.width * 0.5f;
        f.x = std::clamp(f.x, STAGE_LEFT + hw, STAGE_RIGHT - hw);
    }

    // séparation des corps
    Fighter& a = m.f[0];
    Fighter& b = m.f[1];
    auto noPush = [](const Fighter& f) {
        return (f.state == FState::Attack && (getCharacter(f.charId).moves[f.move].flags & MF_NOPUSH)) ||
               f.state == FState::Knockdown || f.state == FState::KO;
    };
    if (!noPush(a) && !noPush(b)) {
        const Box ha = hurtbox(a), hb = hurtbox(b);
        if (ha.y < hb.y + hb.h && hb.y < ha.y + ha.h) {
            const float minDist = (ha.w + hb.w) * 0.5f;
            float dx = b.x - a.x;
            if (std::fabs(dx) < minDist) {
                float s = dx > 0 ? 1.f : (dx < 0 ? -1.f : static_cast<float>(a.facing));
                const float push = (minDist - std::fabs(dx)) * 0.5f;
                a.x -= s * push;
                b.x += s * push;
                const float aw = getCharacter(a.charId).width * 0.5f, bw = getCharacter(b.charId).width * 0.5f;
                // contre un mur : on pousse l'autre
                if (a.x < STAGE_LEFT + aw) { b.x += (STAGE_LEFT + aw) - a.x; a.x = STAGE_LEFT + aw; }
                if (a.x > STAGE_RIGHT - aw) { b.x -= a.x - (STAGE_RIGHT - aw); a.x = STAGE_RIGHT - aw; }
                if (b.x < STAGE_LEFT + bw) { a.x += (STAGE_LEFT + bw) - b.x; b.x = STAGE_LEFT + bw; }
                if (b.x > STAGE_RIGHT - bw) { a.x -= b.x - (STAGE_RIGHT - bw); b.x = STAGE_RIGHT - bw; }
            }
        }
    }
    // distance max (caméra)
    const float dist = std::fabs(a.x - b.x);
    if (dist > MAX_SEPARATION) {
        const float excess = (dist - MAX_SEPARATION) * 0.5f;
        const float s = a.x < b.x ? 1.f : -1.f;
        a.x += s * excess;
        b.x -= s * excess;
    }
}

bool canBlock(const Fighter& d, float dir, uint16_t flags) {
    if (!grounded(d)) return false;
    if (!(d.state == FState::Idle || d.state == FState::Walk || d.state == FState::Crouch || d.state == FState::Blockstun))
        return false;
    const bool holdAway = dir > 0 ? (d.held & IN_RIGHT) : (d.held & IN_LEFT);
    if (!holdAway) return false;
    const bool low = d.held & IN_DOWN;
    if ((flags & MF_LOW) && !low) return false;
    if ((flags & MF_OVERHEAD) && low) return false;
    return true;
}

// Retourne true si le coup a été « consommé » (touché ou paré)
bool applyHit(Match& m, int defIdx, const HitInfo& h, float dir, float sx, float sy) {
    Fighter& d = m.f[defIdx];
    Fighter& a = m.f[1 - defIdx];
    if (h.flags & MF_GRAB) {
        if (!grounded(d) || d.state == FState::Hitstun || d.state == FState::Blockstun ||
            (d.state == FState::Attack && d.stateFrame < 2))
            return false;
    }
    const bool blocked = !(h.flags & MF_GRAB) && canBlock(d, dir, h.flags);
    if (blocked) {
        const int chip = h.special ? std::max(1, h.damage / 6) : 0;
        d.health -= chip;
        setState(d, FState::Blockstun);
        d.stateFrame = 0;
        d.stun = h.blockstun;
        d.vx = dir * std::max(2.f, h.kbx * 0.8f);
        d.crouching = d.held & IN_DOWN;
        a.meter = std::min(METER_MAX, a.meter + 8);
        d.meter = std::min(METER_MAX, d.meter + 14);
        m.hitstop = std::max(m.hitstop, 5);
        addSpark(m, sx, sy, 1);
        return true;
    }
    const float scale = std::max(0.35f, 1.f - 0.12f * static_cast<float>(d.combo));
    const int dmg = std::max(1, static_cast<int>(std::lround(h.damage * scale)));
    d.health -= dmg;
    d.combo++;
    d.comboDamage = (d.combo == 1 ? 0 : d.comboDamage) + dmg;
    if (!h.super) a.meter = std::min(METER_MAX, a.meter + dmg / 2 + 10);
    d.meter = std::min(METER_MAX, d.meter + dmg / 3);
    const bool wasAir = !grounded(d) || d.launched;
    d.state = FState::Hitstun;
    d.stateFrame = 0;
    d.move = -1;
    d.stun = h.hitstun;
    d.vx = dir * h.kbx;
    d.helpless = false;
    d.crouching = false;
    if ((h.flags & (MF_LAUNCH | MF_KNOCKDOWN)) || wasAir) {
        d.launched = true;
        if (h.flags & (MF_LAUNCH | MF_KNOCKDOWN)) d.knockFlag = true;
        d.vy = std::max(h.kby, wasAir ? 6.f : 5.f);
        d.y = std::max(d.y, 1.f);
    }
    const bool big = h.super || h.damage >= 100;
    m.hitstop = std::max(m.hitstop, h.super ? 8 : (big ? 10 : 6));
    addSpark(m, sx, sy, big ? 2 : 0);
    return true;
}

void detectHits(Match& m) {
    // collecte d'abord, application ensuite (les échanges sont possibles)
    struct Pending { bool hit = false; HitInfo h{}; float dir = 0, sx = 0, sy = 0; int attacker = -1; };
    Pending pend[2];
    for (int i = 0; i < 2; ++i) {
        Fighter& f = m.f[i];
        const Fighter& o = m.f[1 - i];
        if (f.state != FState::Attack) continue;
        const MoveDef& mv = curMove(f);
        if (mv.hitbox.w <= 0 || !moveIsActive(f, mv)) continue;
        if (f.hitsDone >= mv.hits) continue;
        if (f.hitsDone > 0 && f.stateFrame - f.lastHitFrame < mv.hitInterval) continue;
        if (isInvulnerable(o)) continue;
        const Box hb = moveHitboxWorld(f, mv);
        const Box hu = hurtbox(o);
        if (!overlap(hb, hu)) continue;
        Pending& p = pend[1 - i];
        p.hit = true;
        p.attacker = i;
        p.h = {mv.damage, mv.hitstun, mv.blockstun, mv.kbx, mv.kby, mv.flags, isSpecialSlot(f.move), f.move == M_SUPER};
        p.dir = static_cast<float>(f.facing);
        const float x0 = std::max(hb.x, hu.x), x1 = std::min(hb.x + hb.w, hu.x + hu.w);
        const float y0 = std::max(hb.y, hu.y), y1 = std::min(hb.y + hb.h, hu.y + hu.h);
        p.sx = (x0 + x1) * 0.5f;
        p.sy = (y0 + y1) * 0.5f;
    }
    // une saisie contre une attaque : la saisie perd si l'autre touche aussi
    for (int d = 0; d < 2; ++d) {
        Pending& p = pend[d];
        if (!p.hit) continue;
        if ((p.h.flags & MF_GRAB) && pend[1 - d].hit) continue;
        Fighter& a = m.f[p.attacker];
        if (applyHit(m, d, p.h, p.dir, p.sx, p.sy)) {
            a.hitsDone++;
            a.lastHitFrame = a.stateFrame;
            if (p.h.flags & MF_GRAB) {
                // la cible est projetée depuis la position de l'attaquant
                m.f[d].x = a.x + a.facing * 30.f;
            }
        }
    }
}

void updateProjectiles(Match& m) {
    for (auto& p : m.proj) {
        if (!p.active) continue;
        p.x += p.vx;
        p.y += p.vy;
        p.age++;
        if (p.cooldown > 0) p.cooldown--;
        if (--p.life <= 0 || p.x + p.w < STAGE_LEFT - 50 || p.x > STAGE_RIGHT + 50 || p.y < 0) p.active = false;
    }
    // projectile contre projectile
    for (int i = 0; i < MAX_PROJ; ++i) {
        for (int j = i + 1; j < MAX_PROJ; ++j) {
            Projectile& a = m.proj[i];
            Projectile& b = m.proj[j];
            if (!a.active || !b.active || a.owner == b.owner) continue;
            if (!overlap({a.x, a.y, a.w, a.h}, {b.x, b.y, b.w, b.h})) continue;
            if (a.cooldown > 0 || b.cooldown > 0) continue;
            a.hitsLeft--;
            b.hitsLeft--;
            a.cooldown = b.cooldown = 4;
            addSpark(m, (a.x + a.w * 0.5f + b.x + b.w * 0.5f) * 0.5f, a.y + a.h * 0.5f, 3);
            if (a.hitsLeft <= 0) a.active = false;
            if (b.hitsLeft <= 0) b.active = false;
        }
    }
}

void projectileHits(Match& m) {
    for (auto& p : m.proj) {
        if (!p.active || p.cooldown > 0) continue;
        const int di = 1 - p.owner;
        Fighter& d = m.f[di];
        if (isInvulnerable(d)) continue;
        const Box pb{p.x, p.y, p.w, p.h};
        if (!overlap(pb, hurtbox(d))) continue;
        const ProjectileDef& pd = getCharacter(p.charId).moves[p.slot].proj;
        HitInfo h{pd.damage, pd.hitstun, pd.blockstun, pd.kbx, pd.kby, pd.flags, true, p.slot == M_SUPER};
        const float dir = p.vx >= 0 ? 1.f : -1.f;
        applyHit(m, di, h, dir, p.x + p.w * 0.5f, p.y + p.h * 0.5f);
        p.hitsLeft--;
        p.cooldown = pd.hitInterval;
        if (p.hitsLeft <= 0) p.active = false;
    }
}

void advanceStates(Match& m) {
    for (int i = 0; i < 2; ++i) {
        Fighter& f = m.f[i];
        if (f.bufferTimer > 0 && --f.bufferTimer == 0) f.buffer = 0;
        if (f.invuln > 0) f.invuln--;
        if (f.stateFrame < 30000) f.stateFrame++;
        switch (f.state) {
        case FState::Attack: {
            const MoveDef& mv = curMove(f);
            if (f.stateFrame >= mv.total()) {
                const bool g = grounded(f);
                const bool fromGround = !isAirSlot(f.move) && !(mv.flags & MF_AIR_OK);
                f.move = -1;
                if (g) {
                    setState(f, FState::Idle);
                    f.vx = 0;
                } else {
                    setState(f, FState::Jump);
                    f.helpless = fromGround;
                }
            }
            break;
        }
        case FState::Hitstun:
            if (!f.launched && grounded(f) && --f.stun <= 0) {
                setState(f, FState::Idle);
                f.combo = 0;
            }
            break;
        case FState::Blockstun:
            if (--f.stun <= 0) setState(f, f.crouching ? FState::Crouch : FState::Idle);
            break;
        case FState::Knockdown:
            if (f.stateFrame >= 40) {
                setState(f, FState::Idle);
                f.invuln = 14;
                f.combo = 0;
                f.vx = 0;
            }
            break;
        default:
            break;
        }
    }
}

void updateFacing(Match& m) {
    for (int i = 0; i < 2; ++i) {
        Fighter& f = m.f[i];
        const Fighter& o = m.f[1 - i];
        if (!grounded(f)) continue;
        if (f.state == FState::Idle || f.state == FState::Walk || f.state == FState::Crouch) {
            if (o.x > f.x + 1) f.facing = 1;
            else if (o.x < f.x - 1) f.facing = -1;
        }
    }
}

void checkRoundEnd(Match& m) {
    const bool ko0 = m.f[0].health <= 0, ko1 = m.f[1].health <= 0;
    const bool timeUp = m.roundTime <= 0;
    if (!ko0 && !ko1 && !timeUp) return;
    if (ko0 || ko1) {
        for (int i = 0; i < 2; ++i) {
            Fighter& f = m.f[i];
            if (f.health > 0) continue;
            f.health = 0;
            f.state = FState::KO;
            f.stateFrame = 0;
            f.vy = 10;
            f.y = std::max(f.y, 1.f);
            f.vx = -static_cast<float>(f.facing) * 5.f;
            f.move = -1;
            addSpark(m, f.x, f.y + 80, 4);
        }
        m.roundWinner = (ko0 && ko1) ? 2 : (ko0 ? 1 : 0);
        m.timeOut = false;
        m.hitstop = 40;
    } else {
        const float r0 = static_cast<float>(m.f[0].health) / getCharacter(m.f[0].charId).maxHealth;
        const float r1 = static_cast<float>(m.f[1].health) / getCharacter(m.f[1].charId).maxHealth;
        m.roundWinner = std::fabs(r0 - r1) < 0.0001f ? 2 : (r0 > r1 ? 0 : 1);
        m.timeOut = true;
    }
    if (m.roundWinner < 2) m.wins[m.roundWinner]++;
    m.phase = Phase::RoundEnd;
    m.phaseTimer = 210;
}

void trainingRegen(Match& m) {
    for (auto& f : m.f) {
        f.meter = METER_MAX;
        const int mx = getCharacter(f.charId).maxHealth;
        if (f.combo == 0 && f.state != FState::Hitstun && f.state != FState::Knockdown) f.health = std::min(mx, f.health + 25);
        f.health = std::max(1, f.health);
    }
    m.roundTime = ROUND_TIME;
}

} // namespace

Box hurtbox(const Fighter& f) {
    const CharacterDef& c = getCharacter(f.charId);
    float h = c.height;
    const bool crouchMove = f.state == FState::Attack && isCrouchSlot(f.move);
    if (f.state == FState::Crouch || crouchMove || (f.state == FState::Blockstun && f.crouching)) h *= 0.62f;
    if (f.state == FState::Knockdown || f.state == FState::KO) h = 40;
    if (f.state == FState::Jump || (f.state == FState::Attack && !grounded(f))) h *= 0.85f;
    return {f.x - c.width * 0.5f, f.y, c.width, h};
}

bool isInvulnerable(const Fighter& f) {
    if (f.state == FState::Knockdown || f.state == FState::KO || f.invuln > 0) return true;
    if (f.state == FState::Attack) {
        const MoveDef& mv = getCharacter(f.charId).moves[f.move];
        if (f.stateFrame >= mv.invulnStart && f.stateFrame < mv.invulnEnd) return true;
    }
    return false;
}

Box moveHitboxWorld(const Fighter& f, const MoveDef& mv) {
    const Box& r = mv.hitbox;
    const float x = f.facing > 0 ? f.x + r.x : f.x - r.x - r.w;
    return {x, f.y + r.y, r.w, r.h};
}

bool moveIsActive(const Fighter& f, const MoveDef& mv) {
    return f.state == FState::Attack && f.stateFrame >= mv.startup && f.stateFrame < mv.startup + mv.active;
}

void startRound(Match& m) {
    for (int i = 0; i < 2; ++i) {
        Fighter& f = m.f[i];
        const int keepMeter = f.meter;
        const int id = f.charId;
        f = Fighter{};
        f.charId = id;
        f.meter = keepMeter;
        f.health = getCharacter(id).maxHealth;
        f.x = i == 0 ? 600.f : 1000.f;
        f.facing = i == 0 ? 1 : -1;
    }
    for (auto& p : m.proj) p.active = false;
    m.roundTime = ROUND_TIME;
    m.phase = Phase::Intro;
    m.phaseTimer = 110;
    m.roundWinner = -1;
    m.timeOut = false;
    m.hitstop = 0;
}

void initMatch(Match& m, int char0, int char1) {
    const bool training = m.training;
    m = Match{};
    m.training = training;
    m.f[0].charId = char0;
    m.f[1].charId = char1;
    m.round = 1;
    startRound(m);
}

void stepMatch(Match& m, const Input in[2]) {
    if (m.phase == Phase::Waiting) return;
    m.frame++;
    for (auto& s : m.sparks)
        if (s.age < 999) s.age++;

    // gel d'impact : on garde les appuis en mémoire
    if (m.hitstop > 0) {
        m.hitstop--;
        for (int i = 0; i < 2; ++i) {
            m.f[i].held = in[i].held;
            if (in[i].pressed) {
                m.f[i].buffer |= in[i].pressed;
                m.f[i].bufferTimer = 8;
            }
        }
        return;
    }

    if (m.phase == Phase::Fight) {
        for (int i = 0; i < 2; ++i) handleInput(m, i, in[i]);
        if (!m.training) m.roundTime--;
    } else {
        for (auto& f : m.f) {
            f.held = 0;
            if (f.state == FState::Walk) { setState(f, FState::Idle); f.vx = 0; }
            if (f.state == FState::Crouch) setState(f, FState::Idle);
        }
    }

    for (int i = 0; i < 2; ++i) applyMoveEffects(m, i);
    physics(m);
    updateProjectiles(m);
    if (m.phase == Phase::Fight) {
        detectHits(m);
        projectileHits(m);
    }
    advanceStates(m);
    updateFacing(m);

    switch (m.phase) {
    case Phase::Intro:
        if (--m.phaseTimer <= 0) m.phase = Phase::Fight;
        break;
    case Phase::Fight:
        if (m.training) trainingRegen(m);
        else checkRoundEnd(m);
        break;
    case Phase::RoundEnd: {
        if (m.roundWinner >= 0 && m.roundWinner < 2) {
            Fighter& w = m.f[m.roundWinner];
            if (grounded(w) && (w.state == FState::Idle || w.state == FState::Crouch) && m.phaseTimer < 150)
                setState(w, FState::Victory);
        }
        if (--m.phaseTimer <= 0) {
            if (m.wins[0] >= ROUNDS_TO_WIN || m.wins[1] >= ROUNDS_TO_WIN) {
                m.phase = Phase::MatchEnd;
                m.phaseTimer = 360;
            } else {
                m.round++;
                startRound(m);
            }
        }
        break;
    }
    case Phase::MatchEnd:
        if (m.phaseTimer > 0) m.phaseTimer--;
        break;
    default:
        break;
    }
}

} // namespace fg
