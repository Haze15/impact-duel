// Fausse raylib pour faire tourner le client sans écran (tests automatiques).
// Un script simule des appuis de touches ; les textes affichés sont enregistrés.
#include "raylib.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Step { int wait; int key; int hold; bool random; bool realtime; const char* label; };
std::vector<Step> g_script;
size_t g_step = 0;
int g_wait = 0;
int g_frame = 0;
bool g_down[512] = {}, g_prev[512] = {};
int g_holdKey = -1, g_holdLeft = 0;
int g_randomLeft = 0;
bool g_realtime = false;
bool g_quit = false;
std::set<std::string> g_texts;
const char* g_label = "debut";

void flushTexts() {
    std::printf("== [%s] textes vus :", g_label);
    for (const auto& t : g_texts) std::printf(" | %s", t.c_str());
    std::printf("\n");
    std::fflush(stdout);
    g_texts.clear();
}

void buildScript(const char* scenario) {
    auto tap = [](int k, int wait = 6, const char* label = nullptr) { g_script.push_back({wait, k, 2, false, false, label}); };
    auto play = [](int frames, const char* label) { g_script.push_back({1, -1, frames, true, false, label}); };
    if (std::strcmp(scenario, "local") == 0) {
        tap(KEY_DOWN, 30, "titre");  // contre l'ordi
        tap(KEY_RIGHT);              // difficulté
        tap(KEY_ENTER, 6);
        tap(KEY_RIGHT, 20, "selection");
        tap(KEY_ENTER);
        tap(KEY_RIGHT);
        tap(KEY_RIGHT);
        tap(KEY_ENTER);
        play(60 * 200, "combat CPU");
        tap(KEY_ENTER, 30, "fin de match"); // revanche
        play(600, "revanche");
        tap(KEY_ESCAPE, 10, "pause");
        tap(KEY_DOWN);
        tap(KEY_DOWN);
        tap(KEY_ENTER);
        tap(KEY_DOWN, 20, "menu");   // entrainement (le menu se souvient de la ligne 1)
        tap(KEY_ENTER);
        tap(KEY_ENTER, 10, "selection entrainement");
        tap(KEY_ENTER);
        play(60 * 20, "entrainement");
        tap(KEY_F2);
        play(60 * 20, "entrainement IA");
        tap(KEY_ESCAPE, 10);
        tap(KEY_DOWN);
        tap(KEY_DOWN);
        tap(KEY_ENTER);
        tap(KEY_UP, 20, "versus");
        tap(KEY_UP);
        tap(KEY_ENTER);              // versus local
        tap(KEY_D, 10);
        tap(KEY_F);
        tap(KEY_LEFT);
        tap(KEY_KP_1);
        play(60 * 30, "versus local");
        tap(KEY_ESCAPE, 10);
        tap(KEY_DOWN);
        tap(KEY_DOWN);
        tap(KEY_ENTER);
        tap(KEY_DOWN, 20, "commandes");
        tap(KEY_DOWN);
        tap(KEY_DOWN);
        tap(KEY_DOWN);
        tap(KEY_ENTER);
        tap(KEY_ESCAPE, 20);
        tap(KEY_DOWN, 10, "quitter");
        tap(KEY_ENTER);
    } else { // online
        g_script.push_back({1, -1, 0, false, true, nullptr});
        tap(KEY_DOWN, 30, "titre");
        tap(KEY_DOWN);
        tap(KEY_DOWN);
        tap(KEY_ENTER);             // en ligne
        tap(KEY_TAB, 10, "setup");
        tap(KEY_ENTER, 10);
        if (std::getenv("STUB_SECOND")) tap(KEY_RIGHT, 10);
        tap(KEY_ENTER, 10, "selection");
        play(60 * 45, "en ligne");
        tap(KEY_ESCAPE, 10, "quitte en ligne");
        tap(KEY_DOWN, 20);
        tap(KEY_DOWN);
        tap(KEY_ENTER);
    }
    g_script.push_back({60, -2, 0, false, false, "fin"});
}

void advance() {
    std::memcpy(g_prev, g_down, sizeof(g_down));
    if (g_holdKey >= 0 && --g_holdLeft <= 0) { g_down[g_holdKey] = false; g_holdKey = -1; }
    if (g_randomLeft > 0) {
        --g_randomLeft;
        static const int keys[] = {KEY_W, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_T, KEY_UP, KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_0};
        if (g_frame % 5 == 0)
            for (int k : keys) g_down[k] = (std::rand() % 4) == 0;
        if (g_randomLeft == 0) for (int k : keys) g_down[k] = false;
        return;
    }
    if (g_wait > 0) { --g_wait; return; }
    if (g_step >= g_script.size()) { g_quit = true; return; }
    const Step& s = g_script[g_step++];
    if (s.label) { flushTexts(); g_label = s.label; }
    if (s.realtime) g_realtime = true;
    g_wait = s.wait;
    if (s.key == -2) { g_quit = true; return; }
    if (s.random) g_randomLeft = s.hold;
    else if (s.key >= 0) { g_down[s.key] = true; g_holdKey = s.key; g_holdLeft = s.hold; }
}
} // namespace

extern "C" {
void SetConfigFlags(unsigned int) {}
void InitWindow(int, int, const char*) {
    const char* sc = std::getenv("STUB_SCENARIO");
    std::srand(std::getenv("STUB_SECOND") ? 7 : 3);
    buildScript(sc ? sc : "local");
}
void SetExitKey(int) {}
void SetWindowMinSize(int, int) {}
void SetTargetFPS(int) {}
bool WindowShouldClose(void) { return g_quit; }
void CloseWindow(void) { flushTexts(); std::printf("== fermeture apres %d frames\n", g_frame); }
void ToggleBorderlessWindowed(void) {}
int GetScreenWidth(void) { return 1280; }
int GetScreenHeight(void) { return 720; }
float GetFrameTime(void) { return 1.f / 60.f; }
double GetTime(void) {
    using namespace std::chrono;
    static const auto t0 = steady_clock::now();
    return duration<double>(steady_clock::now() - t0).count();
}
int GetRandomValue(int a, int b) { return b <= a ? a : a + std::rand() % (b - a + 1); }
void* MemAlloc(unsigned int size) { return std::calloc(size, 1); }

bool IsKeyDown(int k) { return k >= 0 && k < 512 && g_down[k]; }
bool IsKeyPressed(int k) { return k >= 0 && k < 512 && g_down[k] && !g_prev[k]; }
bool IsKeyPressedRepeat(int) { return false; }
int GetCharPressed(void) { return 0; }
bool IsGamepadAvailable(int) { return false; }
bool IsGamepadButtonDown(int, int) { return false; }
bool IsGamepadButtonPressed(int, int) { return false; }
float GetGamepadAxisMovement(int, int) { return 0; }

void BeginDrawing(void) {}
void EndDrawing(void) {
    ++g_frame;
    advance();
    if (g_realtime) std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
void ClearBackground(Color) {}
RenderTexture2D LoadRenderTexture(int, int) { return RenderTexture2D{}; }
void UnloadRenderTexture(RenderTexture2D) {}
void SetTextureFilter(Texture2D, int) {}
void BeginTextureMode(RenderTexture2D) {}
void EndTextureMode(void) {}
void DrawTexturePro(Texture2D, Rectangle, Rectangle, Vector2, float, Color) {}

Color Fade(Color c, float a) {
    if (a < 0) a = 0;
    if (a > 1) a = 1;
    c.a = static_cast<unsigned char>(255 * a);
    return c;
}
void DrawCircle(int, int, float, Color) {}
void DrawCircleV(Vector2, float, Color) {}
void DrawEllipse(int, int, float, float, Color) {}
void DrawLineEx(Vector2 a, Vector2 b, float, Color) {
    if (a.x != a.x || b.y != b.y) { std::printf("NaN dans DrawLineEx !\n"); std::abort(); }
}
void DrawPixel(int, int, Color) {}
void DrawPoly(Vector2, int, float, float, Color) {}
void DrawRectangle(int, int, int, int, Color) {}
void DrawRectangleGradientV(int, int, int, int, Color, Color) {}
void DrawRectangleLinesEx(Rectangle, float, Color) {}
void DrawRectangleRounded(Rectangle, float, int, Color) {}
void DrawRectangleRoundedLinesEx(Rectangle, float, int, float, Color) {}
void DrawRing(Vector2, float, float, float, float, int, Color) {}
void DrawTriangle(Vector2, Vector2, Vector2, Color) {}
void DrawText(const char* t, int, int, int, Color) {
    if (!t) return;
    std::string s(t);
    for (char& ch : s) if (ch >= '0' && ch <= '9') ch = '#'; // regroupe les nombres
    if (g_texts.size() < 400) g_texts.insert(s);
}
int MeasureText(const char* t, int size) { return t ? static_cast<int>(std::strlen(t)) * size / 2 : 0; }
const char* TextFormat(const char* fmt, ...) {
    static char bufs[4][1024];
    static int idx = 0;
    char* b = bufs[idx];
    idx = (idx + 1) % 4;
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(b, 1024, fmt, ap);
    va_end(ap);
    return b;
}

void InitAudioDevice(void) {}
void CloseAudioDevice(void) {}
bool IsAudioDeviceReady(void) { return true; }
Sound LoadSoundFromWave(Wave) { return Sound{}; }
void UnloadSound(Sound) {}
void UnloadWave(Wave w) { std::free(w.data); }
void PlaySound(Sound) {}
}
