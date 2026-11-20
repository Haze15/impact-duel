// IMPACT DUEL — client graphique (raylib)
// Modes : versus local, contre l'ordi, entraînement, en ligne (serveur UDP).
#include "client/ai.h"
#include "client/audio.h"
#include "client/render.h"
#include "core/protocol.h"
#include "core/sim.h"
#include "net/net.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace fg;
using namespace client;

namespace {

// ---------------------------------------------------------------------------
// Entrées
// ---------------------------------------------------------------------------
struct KeySet {
    int up, down, left, right, a, b, c, d;
};
// raylib utilise les positions physiques US : W/A/S/D = Z/Q/S/D sur AZERTY.
const KeySet P1_KEYS{KEY_W, KEY_S, KEY_A, KEY_D, KEY_F, KEY_G, KEY_H, KEY_T};
const KeySet P2_KEYS{KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_0};
// alternative sans pavé numérique (positions US , . / et Maj droite = ; : ! sur AZERTY)
const KeySet P2_ALT{KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_COMMA, KEY_PERIOD, KEY_SLASH, KEY_RIGHT_SHIFT};

uint8_t readKeys(const KeySet& k) {
    uint8_t h = 0;
    if (IsKeyDown(k.up)) h |= IN_UP;
    if (IsKeyDown(k.down)) h |= IN_DOWN;
    if (IsKeyDown(k.left)) h |= IN_LEFT;
    if (IsKeyDown(k.right)) h |= IN_RIGHT;
    if (IsKeyDown(k.a)) h |= IN_A;
    if (IsKeyDown(k.b)) h |= IN_B;
    if (IsKeyDown(k.c)) h |= IN_C;
    if (IsKeyDown(k.d)) h |= IN_D;
    return h;
}

uint8_t readPad(int pad) {
    if (!IsGamepadAvailable(pad)) return 0;
    uint8_t h = 0;
    const float ax = GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_X);
    const float ay = GetGamepadAxisMovement(pad, GAMEPAD_AXIS_LEFT_Y);
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_UP) || ay < -0.5f) h |= IN_UP;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_DOWN) || ay > 0.5f) h |= IN_DOWN;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_LEFT) || ax < -0.5f) h |= IN_LEFT;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT) || ax > 0.5f) h |= IN_RIGHT;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) h |= IN_A;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) h |= IN_B;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) h |= IN_C;
    if (IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_UP) || IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_TRIGGER_2)) h |= IN_D;
    return h;
}

uint8_t pollHeld(int player, bool solo) {
    uint8_t h;
    if (solo) h = static_cast<uint8_t>(readKeys(P1_KEYS) | readKeys(P2_KEYS) | readKeys(P2_ALT) | readPad(0));
    else if (player == 0) h = static_cast<uint8_t>(readKeys(P1_KEYS) | readPad(0));
    else h = static_cast<uint8_t>(readKeys(P2_KEYS) | readKeys(P2_ALT) | readPad(1));
    if ((h & IN_LEFT) && (h & IN_RIGHT)) h &= static_cast<uint8_t>(~(IN_LEFT | IN_RIGHT)); // annulation
    return h;
}

bool menuUp() { return IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP); }
bool menuDown() { return IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN); }
bool menuLeft() { return IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT); }
bool menuRight() { return IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT); }
bool menuOk() {
    return IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_F) || IsKeyPressed(KEY_SPACE) ||
           IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT);
}
bool menuBack() { return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT); }

// ---------------------------------------------------------------------------
// Client réseau
// ---------------------------------------------------------------------------
struct OnlineClient {
    net::UdpSocket sock;
    net::Address server;
    bool active = false;
    int slot = -1;
    double lastRecv = 0, lastHello = 0, started = 0;
    uint32_t seq = 0;
    uint8_t counts[4] = {0, 0, 0, 0};
    StatePacket state;
    bool haveState = false;
    std::string error;
    int charId = 0;
    std::string name;

    bool connect(const std::string& addr, int ch, const std::string& nm) {
        disconnect();
        error.clear();
        if (!net::resolve(addr, DEFAULT_PORT, server)) { error = "Adresse introuvable : " + addr; return false; }
        if (!sock.open(0)) { error = "Impossible d'ouvrir un socket UDP"; return false; }
        active = true;
        slot = -1;
        haveState = false;
        seq = 0;
        std::fill(std::begin(counts), std::end(counts), uint8_t{0});
        charId = ch;
        name = nm;
        started = lastRecv = GetTime();
        lastHello = -10;
        return true;
    }
    void disconnect() {
        if (active) {
            const auto bye = makeSimple(PT_BYE);
            sock.send(server, bye.data(), bye.size());
        }
        sock.close();
        active = false;
        slot = -1;
        haveState = false;
    }
    void update() {
        if (!active) return;
        const double now = GetTime();
        uint8_t buf[4096];
        for (;;) {
            net::Address from;
            const int n = sock.receive(from, buf, sizeof(buf));
            if (n <= 0) break;
            if (from != server) continue;
            Reader r(buf, static_cast<size_t>(n));
            PacketType t;
            if (!readHeader(r, t)) continue;
            lastRecv = now;
            if (t == PT_WELCOME) {
                slot = r.u8() & 1;
            } else if (t == PT_FULL) {
                error = "Le serveur est plein (2 joueurs deja connectes)";
                disconnect();
                return;
            } else if (t == PT_STATE) {
                StatePacket sp;
                if (parseState(r, sp) && (!haveState || sp.serverFrame > state.serverFrame)) {
                    state = sp;
                    haveState = true;
                }
            } else if (t == PT_BYE) {
                error = "Le serveur s'est arrete";
                disconnect();
                return;
            }
        }
        if (slot < 0 && now - lastHello > 0.5) {
            const auto pkt = makeHello(charId, name);
            sock.send(server, pkt.data(), pkt.size());
            lastHello = now;
        }
        if (now - lastRecv > 5.0) {
            error = slot < 0 ? "Aucune reponse du serveur (adresse, port UDP, pare-feu ?)" : "Connexion perdue";
            disconnect();
        }
    }
    void sendInput(uint8_t held, uint8_t pressed) {
        if (!active || slot < 0) return;
        for (int b = 0; b < 4; ++b)
            if (pressed & (IN_A << b)) counts[b]++;
        InputPacket ip;
        ip.seq = ++seq;
        ip.held = held;
        for (int b = 0; b < 4; ++b) ip.pressCount[b] = counts[b];
        const auto pkt = makeInput(ip);
        sock.send(server, pkt.data(), pkt.size());
    }
};

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------
enum class Screen { Title, Controls, Select, Fight, OnlineSetup, Online };
enum class Mode { Versus, Cpu, Training, Online };

struct App {
    Screen screen = Screen::Title;
    Mode mode = Mode::Versus;
    int menu = 0;
    int pauseMenu = 0;
    bool paused = false;
    int cursor[2] = {0, 1};
    bool locked[2] = {false, false};
    int cpuLevel = 2;
    int dummyMode = 1; // 0 immobile, 1 garde, 2 IA
    bool hitboxes = false;
    Match match;
    HudMemory hud;
    AIState ai;
    uint8_t prevHeld[2] = {0, 0};
    uint8_t pending[2] = {0, 0};
    uint8_t held[2] = {0, 0};
    uint32_t lastSpark = 0;
    int frame = 0;
    std::string addr = "127.0.0.1:7777";
    std::string playerName = "Joueur";
    int field = 0;
    OnlineClient online;
    bool quit = false;
};

const char* levelName(int l) { return l == 1 ? "FACILE" : (l == 2 ? "NORMAL" : "DIFFICILE"); }

void playMatchSounds(const Match& m, uint32_t& last) {
    if (m.sparkSeq < last) last = 0; // nouveau match
    for (const auto& s : m.sparks) {
        if (s.seq <= last || s.age > 15) continue;
        switch (s.type) {
        case 0: playSfx(SFX_HIT); break;
        case 1: playSfx(SFX_BLOCK); break;
        case 2: playSfx(SFX_HEAVY); break;
        case 3: playSfx(SFX_CLASH); break;
        case 4: playSfx(SFX_KO); break;
        default: break;
        }
    }
    last = std::max(last, m.sparkSeq);
}

void startLocalMatch(App& app) {
    app.match = Match{};
    app.match.training = app.mode == Mode::Training;
    initMatch(app.match, app.cursor[0], app.cursor[1]);
    app.hud = HudMemory{};
    app.ai = AIState{};
    app.ai.level = app.cpuLevel;
    app.ai.rng = static_cast<uint32_t>(GetRandomValue(1, 1 << 30));
    app.paused = false;
    app.lastSpark = 0;
    app.screen = Screen::Fight;
}

void goSelect(App& app, Mode mode) {
    app.mode = mode;
    app.locked[0] = app.locked[1] = false;
    app.screen = Screen::Select;
}

// --- écran titre -------------------------------------------------------------
void updateTitle(App& app) {
    constexpr int N = 6;
    if (menuUp()) { app.menu = (app.menu + N - 1) % N; playSfx(SFX_SELECT); }
    if (menuDown()) { app.menu = (app.menu + 1) % N; playSfx(SFX_SELECT); }
    if (app.menu == 1 && menuLeft()) { app.cpuLevel = std::max(1, app.cpuLevel - 1); playSfx(SFX_SELECT); }
    if (app.menu == 1 && menuRight()) { app.cpuLevel = std::min(3, app.cpuLevel + 1); playSfx(SFX_SELECT); }
    if (menuOk()) {
        playSfx(SFX_CONFIRM);
        switch (app.menu) {
        case 0: goSelect(app, Mode::Versus); break;
        case 1: goSelect(app, Mode::Cpu); break;
        case 2: goSelect(app, Mode::Training); break;
        case 3: app.screen = Screen::OnlineSetup; app.field = 0; break;
        case 4: app.screen = Screen::Controls; break;
        case 5: app.quit = true; break;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) app.quit = true;
}

void drawTitle(const App& app) {
    drawStageBackground(800 + std::sin(app.frame * 0.005f) * 150, app.frame);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.35f));
    drawFighterPreview((app.frame / 240) % characterCount(), {230, GROUND_Y}, 1.5f, app.frame, 1);
    drawFighterPreview((app.frame / 240 + 2) % characterCount(), {1050, GROUND_Y}, 1.5f, app.frame + 30, -1);
    drawTextOutlined("IMPACT DUEL", VW / 2 - MeasureText("IMPACT DUEL", 110) / 2, 70, 110, Color{255, 200, 60, 255},
                     Color{90, 20, 30, 255});
    drawTextCentered("jeu de combat 2D  -  C++ / raylib", VW / 2, 190, 24, Fade(WHITE, 0.8f));
    const char* items[6] = {"VERSUS LOCAL (2 JOUEURS)", nullptr, "ENTRAINEMENT", "EN LIGNE", "COMMANDES", "QUITTER"};
    for (int i = 0; i < 6; ++i) {
        const char* t = i == 1 ? TextFormat("CONTRE L'ORDI   < %s >", levelName(app.cpuLevel)) : items[i];
        const bool sel = app.menu == i;
        const int y = 270 + i * 56;
        if (sel) DrawRectangleRounded({VW / 2.f - 260, y - 8.f, 520, 46}, 0.4f, 8, Fade(Color{255, 200, 60, 255}, 0.25f));
        drawTextCentered(t, VW / 2, y, 30, sel ? Color{255, 220, 90, 255} : WHITE);
    }
    drawTextCentered("Haut/Bas : choisir   Entree : valider   F11 : plein ecran", VW / 2, VH - 34, 18, Fade(WHITE, 0.6f));
}

// --- commandes ---------------------------------------------------------------
void drawControls() {
    drawStageBackground(800, 0);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.7f));
    drawTextCentered("COMMANDES", VW / 2, 40, 54, Color{255, 200, 60, 255});
    const char* left[] = {
        "JOUEUR 1",
        "Deplacement : Z Q S D (AZERTY) / W A S D (QWERTY)",
        "A  coup leger  ........ F",
        "B  coup lourd  ........ G",
        "C  special  ........... H",
        "D  super (jauge) ...... T",
        "",
        "JOUEUR 2",
        "Deplacement : fleches",
        "A B C D : pave num 1 2 3 0",
        "  ou  ; : ! et Maj droite (AZERTY)",
        "",
        "Manette : A/X/B/Y (+ gachette droite = super)",
    };
    const char* right[] = {
        "SYSTEME",
        "Haut : sauter (Nyx : double saut)",
        "Bas : s'accroupir",
        "Arriere : garde (bas+arriere = garde basse)",
        "Les coups bas se parent accroupi,",
        "les attaques sautees se parent debout.",
        "C seul / Avant + C / Bas + C : 3 speciaux",
        "Normal qui touche -> annulable en special",
        "Saisies : imparables, battues par un coup",
        "La jauge se remplit en frappant et en encaissant",
        "",
        "F1 : hitboxes   Echap : pause",
        "Entrainement : F2 mannequin, R replacer",
    };
    for (int i = 0; i < 13; ++i) {
        DrawText(left[i], 70, 130 + i * 38, 22, i == 0 || i == 7 ? Color{255, 220, 90, 255} : WHITE);
        DrawText(right[i], 680, 130 + i * 38, 22, i == 0 ? Color{255, 220, 90, 255} : WHITE);
    }
    drawTextCentered("Echap : retour", VW / 2, VH - 40, 20, Fade(WHITE, 0.6f));
}

// --- sélection des persos ------------------------------------------------------
void updateSelect(App& app) {
    const int n = characterCount();
    const bool twoHumans = app.mode == Mode::Versus;
    // en solo, le joueur 1 choisit aussi l'adversaire après avoir validé
    const int active = (!twoHumans && app.locked[0]) ? 1 : 0;

    auto moveCursor = [&](int p, int d) { app.cursor[p] = (app.cursor[p] + d + n) % n; playSfx(SFX_SELECT); };
    if (twoHumans) {
        if (!app.locked[0]) {
            if (IsKeyPressed(KEY_A) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) moveCursor(0, -1);
            if (IsKeyPressed(KEY_D) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) moveCursor(0, 1);
            if (IsKeyPressed(KEY_F) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) { app.locked[0] = true; playSfx(SFX_CONFIRM); }
        } else if (IsKeyPressed(KEY_G)) app.locked[0] = false;
        if (!app.locked[1]) {
            if (IsKeyPressed(KEY_LEFT) || IsGamepadButtonPressed(1, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) moveCursor(1, -1);
            if (IsKeyPressed(KEY_RIGHT) || IsGamepadButtonPressed(1, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) moveCursor(1, 1);
            if (IsKeyPressed(KEY_KP_1) || IsKeyPressed(KEY_COMMA) || IsKeyPressed(KEY_ENTER) ||
                IsGamepadButtonPressed(1, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) { app.locked[1] = true; playSfx(SFX_CONFIRM); }
        } else if (IsKeyPressed(KEY_KP_2) || IsKeyPressed(KEY_PERIOD)) app.locked[1] = false;
    } else {
        if (menuLeft()) moveCursor(active, -1);
        if (menuRight()) moveCursor(active, 1);
        if (menuOk()) {
            playSfx(SFX_CONFIRM);
            if (app.mode == Mode::Online) app.locked[0] = app.locked[1] = true;
            else app.locked[active] = true;
        }
        if (IsKeyPressed(KEY_BACKSPACE) && app.locked[0] && !app.locked[1]) app.locked[0] = false;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        app.screen = app.mode == Mode::Online ? Screen::OnlineSetup : Screen::Title;
        return;
    }
    if (app.locked[0] && app.locked[1]) {
        if (app.mode == Mode::Online) {
            if (app.online.connect(app.addr, app.cursor[0], app.playerName)) {
                app.hud = HudMemory{};
                app.lastSpark = 0;
                app.screen = Screen::Online;
            } else {
                app.screen = Screen::OnlineSetup;
            }
        } else {
            startLocalMatch(app);
        }
    }
}

void drawStatBar(const char* label, int v, int x, int y, Color c) {
    DrawText(label, x, y, 18, LIGHTGRAY);
    for (int i = 0; i < 5; ++i) DrawRectangle(x + 90 + i * 30, y + 2, 26, 14, i < v ? c : Color{60, 50, 70, 255});
}

void drawCharPanel(int charId, int x, int y, Color tint, std::string who) {
    const CharacterDef& c = getCharacter(charId);
    DrawRectangleRounded({float(x), float(y), 590, 340}, 0.05f, 6, Fade(BLACK, 0.6f));
    DrawRectangleRoundedLinesEx({float(x), float(y), 590, 340}, 0.05f, 6, 2, tint);
    DrawText(who.c_str(), x + 16, y + 12, 18, tint);
    DrawText(c.name, x + 16, y + 34, 40, WHITE);
    DrawText(c.title, x + 18 + MeasureText(c.name, 40) + 12, y + 50, 20, toColor(c.accent));
    DrawText(c.desc, x + 16, y + 82, 16, LIGHTGRAY);
    drawStatBar("PUISSANCE", c.statPower, x + 16, y + 108, tint);
    drawStatBar("VITESSE", c.statSpeed, x + 16, y + 130, tint);
    drawStatBar("PORTEE", c.statRange, x + 16, y + 152, tint);
    DrawText(TextFormat("VIE %d", c.maxHealth), x + 280, y + 110, 18, LIGHTGRAY);
    for (int i = 0; i < M_COUNT; ++i) {
        const MoveDef& mv = c.moves[i];
        const int col = i < 5 ? 0 : 1;
        const int row = i % 5;
        const int mx = x + 16 + col * 290, my = y + 186 + row * 30;
        DrawText(mv.input, mx, my, 16, i >= M_SP_NEUTRAL ? Color{255, 210, 90, 255} : Color{170, 200, 255, 255});
        DrawText(mv.name, mx + 4, my + 15, 14, WHITE);
    }
}

void drawSelect(const App& app) {
    drawStageBackground(800, app.frame);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.55f));
    const bool solo = app.mode != Mode::Versus;
    const char* title = app.mode == Mode::Online ? "CHOISIS TON COMBATTANT"
                        : (solo ? (app.locked[0] ? "CHOISIS TON ADVERSAIRE" : "CHOISIS TON COMBATTANT")
                                : "SELECTION DES COMBATTANTS");
    drawTextCentered(title, VW / 2, 18, 40, Color{255, 200, 60, 255});
    const int n = characterCount();
    const Color c1{255, 90, 80, 255}, c2{80, 160, 255, 255};
    for (int i = 0; i < n; ++i) {
        const float cx = VW / 2.f + (i - (n - 1) / 2.f) * 250.f;
        const Rectangle card{cx - 110, 70, 220, 230};
        DrawRectangleRounded(card, 0.1f, 6, Fade(toColor(getCharacter(i).color2), 0.5f));
        drawFighterPreview(i, {cx, 270}, 1.0f, app.frame + i * 20, 1);
        drawTextCentered(getCharacter(i).name, static_cast<int>(cx), 272, 24, WHITE);
        const bool show2 = app.mode != Mode::Online && (!solo || app.locked[0]);
        if (app.cursor[0] == i) DrawRectangleRoundedLinesEx(card, 0.1f, 6, app.locked[0] ? 6.f : 3.f, c1);
        if (show2 && app.cursor[1] == i)
            DrawRectangleRoundedLinesEx({card.x - 6, card.y - 6, card.width + 12, card.height + 12}, 0.1f, 6, app.locked[1] ? 6.f : 3.f, c2);
        if (app.cursor[0] == i) DrawText(solo ? "TOI" : "J1", static_cast<int>(card.x + 8), 76, 20, c1);
        if (show2 && app.cursor[1] == i)
            DrawText(solo ? "CPU" : "J2", static_cast<int>(card.x + card.width - 40), 76, 20, c2);
    }
    drawCharPanel(app.cursor[0], 40, 320, c1, app.locked[0] ? "JOUEUR 1 - PRET !" : "JOUEUR 1");
    if (app.mode != Mode::Online)
        drawCharPanel(app.cursor[1], VW - 40 - 590, 320, c2,
                      solo ? (app.mode == Mode::Training ? "MANNEQUIN" : TextFormat("ORDI (%s)", levelName(app.cpuLevel)))
                           : (app.locked[1] ? "JOUEUR 2 - PRET !" : "JOUEUR 2"));
    else {
        drawTextCentered(TextFormat("Serveur : %s", app.addr.c_str()), VW / 2 + 320, 420, 22, WHITE);
        drawTextCentered(TextFormat("Pseudo : %s", app.playerName.c_str()), VW / 2 + 320, 456, 22, WHITE);
    }
    const char* hint = solo ? "Gauche/Droite : choisir   Entree : valider   Echap : retour"
                            : "J1 : Q/D + F (G annule)     J2 : fleches + 1 ou ; (2 annule)     Echap : retour";
    drawTextCentered(hint, VW / 2, VH - 22, 16, Fade(WHITE, 0.7f));
}

// --- combat local --------------------------------------------------------------
Input dummyInput(const App& app) {
    Input in;
    if (app.dummyMode == 2) return in; // géré par l'IA
    if (app.dummyMode == 1) {
        const Fighter& me = app.match.f[1];
        const Fighter& p = app.match.f[0];
        in.held = me.facing > 0 ? IN_LEFT : IN_RIGHT;
        if (p.state == FState::Attack && p.move >= 0 && (getCharacter(p.charId).moves[p.move].flags & MF_LOW)) in.held |= IN_DOWN;
    }
    return in;
}

void simStepLocal(App& app) {
    if (app.paused) return;
    Input in[2];
    const bool solo = app.mode != Mode::Versus;
    in[0].held = app.held[0];
    in[0].pressed = app.pending[0];
    if (app.mode == Mode::Versus) {
        in[1].held = app.held[1];
        in[1].pressed = app.pending[1];
    } else if (app.mode == Mode::Cpu || (app.mode == Mode::Training && app.dummyMode == 2)) {
        in[1] = aiInput(app.match, 1, app.ai);
    } else {
        in[1] = dummyInput(app);
    }
    app.pending[0] = app.pending[1] = 0;
    (void)solo;
    stepMatch(app.match, in);
}

void updateFight(App& app) {
    if (IsKeyPressed(KEY_ESCAPE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
        app.paused = !app.paused;
        app.pauseMenu = 0;
    }
    if (app.paused) {
        if (menuUp()) app.pauseMenu = (app.pauseMenu + 2) % 3;
        if (menuDown()) app.pauseMenu = (app.pauseMenu + 1) % 3;
        if (IsKeyPressed(KEY_ENTER) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            if (app.pauseMenu == 0) app.paused = false;
            else if (app.pauseMenu == 1) goSelect(app, app.mode);
            else app.screen = Screen::Title;
        }
        return;
    }
    if (app.mode == Mode::Training) {
        if (IsKeyPressed(KEY_F2)) app.dummyMode = (app.dummyMode + 1) % 3;
        if (IsKeyPressed(KEY_R)) {
            initMatch(app.match, app.cursor[0], app.cursor[1]);
            app.match.phase = Phase::Fight;
        }
    }
    if (app.match.phase == Phase::MatchEnd && app.match.phaseTimer < 300) {
        if (IsKeyPressed(KEY_ENTER) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) startLocalMatch(app);
        if (IsKeyPressed(KEY_C)) goSelect(app, app.mode);
        if (IsKeyPressed(KEY_BACKSPACE)) app.screen = Screen::Title;
    }
}

void drawFight(App& app) {
    RenderOpts o;
    o.hitboxes = app.hitboxes;
    if (app.mode == Mode::Versus) { o.names[0] = "J1"; o.names[1] = "J2"; }
    else if (app.mode == Mode::Cpu) { o.names[0] = "TOI"; o.names[1] = TextFormat("CPU %s", levelName(app.cpuLevel)); }
    else { o.names[0] = "TOI"; o.names[1] = "MANNEQUIN"; }
    if (app.mode == Mode::Training) {
        static const char* dm[3] = {"immobile", "garde", "IA"};
        o.bottomHint = TextFormat("ENTRAINEMENT  -  F2 mannequin : %s   R : replacer   F1 : hitboxes   Echap : pause", dm[app.dummyMode]);
    }
    drawMatch(app.match, o, app.hud);
    if (app.match.phase == Phase::MatchEnd && app.match.phaseTimer < 300)
        drawTextCentered("Entree : revanche     C : changer de persos     Retour arriere : menu", VW / 2, 330, 24, WHITE);
    if (app.paused) {
        DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
        drawTextCentered("PAUSE", VW / 2, 200, 70, Color{255, 200, 60, 255});
        const char* items[3] = {"REPRENDRE", "CHANGER DE PERSOS", "MENU PRINCIPAL"};
        for (int i = 0; i < 3; ++i)
            drawTextCentered(items[i], VW / 2, 320 + i * 50, 32, app.pauseMenu == i ? Color{255, 220, 90, 255} : WHITE);
    }
}

// --- en ligne --------------------------------------------------------------------
void updateOnlineSetup(App& app) {
    if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)) app.field = 1 - app.field;
    std::string& s = app.field == 0 ? app.addr : app.playerName;
    for (int ch = GetCharPressed(); ch > 0; ch = GetCharPressed())
        if (ch >= 32 && ch < 127 && s.size() < (app.field == 0 ? 60u : 15u)) s.push_back(static_cast<char>(ch));
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !s.empty()) s.pop_back();
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
        if (app.playerName.empty()) app.playerName = "Joueur";
        playSfx(SFX_CONFIRM);
        goSelect(app, Mode::Online);
    }
    if (IsKeyPressed(KEY_ESCAPE)) app.screen = Screen::Title;
}

void drawOnlineSetup(const App& app) {
    drawStageBackground(800, app.frame);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
    drawTextCentered("JOUER EN LIGNE", VW / 2, 80, 60, Color{255, 200, 60, 255});
    const char* labels[2] = {"ADRESSE DU SERVEUR (ip:port)", "PSEUDO"};
    const std::string* vals[2] = {&app.addr, &app.playerName};
    for (int i = 0; i < 2; ++i) {
        const int y = 220 + i * 120;
        DrawText(labels[i], VW / 2 - 300, y, 22, LIGHTGRAY);
        const Rectangle r{VW / 2.f - 300, y + 30.f, 600, 54};
        DrawRectangleRounded(r, 0.2f, 6, Fade(BLACK, 0.7f));
        DrawRectangleRoundedLinesEx(r, 0.2f, 6, 2, app.field == i ? Color{255, 200, 60, 255} : GRAY);
        const std::string t = *vals[i] + ((app.field == i && (app.frame / 30) % 2) ? "_" : "");
        DrawText(t.c_str(), static_cast<int>(r.x) + 14, static_cast<int>(r.y) + 12, 30, WHITE);
    }
    if (!app.online.error.empty()) drawTextCentered(app.online.error.c_str(), VW / 2, 480, 22, Color{255, 110, 100, 255});
    drawTextCentered("Lance d'abord le serveur :  impact_server 7777", VW / 2, 540, 20, Fade(WHITE, 0.7f));
    drawTextCentered("Tab : changer de champ    Entree : choisir son perso    Echap : retour", VW / 2, VH - 40, 20,
                     Fade(WHITE, 0.6f));
}

void updateOnline(App& app) {
    if (IsKeyPressed(KEY_ESCAPE)) {
        app.online.disconnect();
        app.online.error.clear();
        app.screen = Screen::Title;
        return;
    }
    app.online.update();
    if (!app.online.active) {
        app.screen = Screen::OnlineSetup;
        return;
    }
    if (app.online.haveState) playMatchSounds(app.online.state.match, app.lastSpark);
}

void drawOnline(App& app) {
    if (!app.online.haveState) {
        drawStageBackground(800, app.frame);
        DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.6f));
        const char* dots = &"..."[2 - (app.frame / 20) % 3];
        drawTextCentered(TextFormat("Connexion a %s %s", app.addr.c_str(), dots), VW / 2, 320, 36, WHITE);
        drawTextCentered("Echap : annuler", VW / 2, VH - 40, 20, Fade(WHITE, 0.6f));
        return;
    }
    const StatePacket& s = app.online.state;
    RenderOpts o;
    o.hitboxes = app.hitboxes;
    o.localSlot = app.online.slot;
    for (int i = 0; i < 2; ++i) o.names[i] = s.names[i];
    o.bottomHint = s.match.phase == Phase::MatchEnd ? "Revanche automatique dans quelques secondes...  (Echap : quitter)"
                                                    : "EN LIGNE  -  Echap : quitter";
    drawMatch(s.match, o, app.hud);
    if (s.match.phase == Phase::Waiting) {
        drawTextCentered(TextFormat("Tu es le joueur %d. Donne l'adresse du serveur a ton adversaire.", app.online.slot + 1), VW / 2,
                         380, 22, WHITE);
    }
}

} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(VW, VH, "IMPACT DUEL");
    SetExitKey(KEY_NULL);
    SetWindowMinSize(640, 360);
    SetTargetFPS(60);
    net::init();
    audioInit();

    RenderTexture2D target = LoadRenderTexture(VW, VH);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    App app;
    double acc = 0;
    const double dt = 1.0 / FPS;

    while (!WindowShouldClose() && !app.quit) {
        app.frame++;
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_F1)) app.hitboxes = !app.hitboxes;

        // entrées : on accumule les appuis entre deux pas de simulation
        const bool solo = app.mode != Mode::Versus;
        for (int p = 0; p < 2; ++p) {
            app.held[p] = pollHeld(p, solo);
            app.pending[p] |= static_cast<uint8_t>(app.held[p] & ~app.prevHeld[p]);
            app.prevHeld[p] = app.held[p];
        }

        switch (app.screen) {
        case Screen::Title: updateTitle(app); break;
        case Screen::Controls: if (menuBack() || menuOk()) app.screen = Screen::Title; break;
        case Screen::Select: updateSelect(app); break;
        case Screen::Fight: updateFight(app); break;
        case Screen::OnlineSetup: updateOnlineSetup(app); break;
        case Screen::Online: updateOnline(app); break;
        }

        acc += std::min(0.25, static_cast<double>(GetFrameTime()));
        while (acc >= dt) {
            acc -= dt;
            if (app.screen == Screen::Fight) {
                simStepLocal(app);
                playMatchSounds(app.match, app.lastSpark);
            } else if (app.screen == Screen::Online) {
                app.online.sendInput(app.held[0], app.pending[0]);
                app.pending[0] = app.pending[1] = 0;
            } else {
                app.pending[0] = app.pending[1] = 0;
            }
        }

        BeginTextureMode(target);
        ClearBackground(BLACK);
        switch (app.screen) {
        case Screen::Title: drawTitle(app); break;
        case Screen::Controls: drawControls(); break;
        case Screen::Select: drawSelect(app); break;
        case Screen::Fight: drawFight(app); break;
        case Screen::OnlineSetup: drawOnlineSetup(app); break;
        case Screen::Online: drawOnline(app); break;
        }
        EndTextureMode();

        BeginDrawing();
        ClearBackground(BLACK);
        const float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
        const float scale = std::min(sw / VW, sh / VH);
        const Rectangle dst{(sw - VW * scale) * 0.5f, (sh - VH * scale) * 0.5f, VW * scale, VH * scale};
        DrawTexturePro(target.texture, {0, 0, static_cast<float>(VW), -static_cast<float>(VH)}, dst, {0, 0}, 0, WHITE);
        EndDrawing();
    }

    app.online.disconnect();
    UnloadRenderTexture(target);
    audioShutdown();
    CloseWindow();
    net::shutdown();
    return 0;
}
