// Sons générés par synthèse au lancement : aucun fichier audio nécessaire.
#include "client/audio.h"
#include "raylib.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace client {

static Sound g_sounds[SFX_COUNT];
static bool g_ready = false;

static Sound synth(float seconds, float (*gen)(float t, float p)) {
    const int rate = 22050;
    const int n = static_cast<int>(seconds * rate);
    auto* data = static_cast<short*>(MemAlloc(static_cast<unsigned>(n * sizeof(short))));
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) / rate;
        const float p = static_cast<float>(i) / static_cast<float>(n); // progression 0..1
        float v = gen(t, p);
        v = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
        data[i] = static_cast<short>(v * 32000.f);
    }
    Wave w{};
    w.frameCount = static_cast<unsigned>(n);
    w.sampleRate = rate;
    w.sampleSize = 16;
    w.channels = 1;
    w.data = data;
    Sound s = LoadSoundFromWave(w);
    UnloadWave(w);
    return s;
}

static float noise() { return static_cast<float>(std::rand()) / RAND_MAX * 2.f - 1.f; }
static float sq(float x) { return std::sin(x) > 0 ? 1.f : -1.f; }

void audioInit() {
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;
    g_sounds[SFX_HIT] = synth(0.12f, [](float t, float p) {
        return (noise() * 0.7f + std::sin(t * 2 * PI * 180) * 0.6f) * (1 - p) * (1 - p);
    });
    g_sounds[SFX_HEAVY] = synth(0.25f, [](float t, float p) {
        return (noise() * 0.6f + std::sin(t * 2 * PI * (110 - 60 * p)) * 0.9f) * (1 - p);
    });
    g_sounds[SFX_BLOCK] = synth(0.08f, [](float t, float p) {
        return sq(t * 2 * PI * 900) * 0.35f * (1 - p) + noise() * 0.2f * (1 - p);
    });
    g_sounds[SFX_CLASH] = synth(0.2f, [](float t, float p) {
        return (std::sin(t * 2 * PI * 1200) * 0.4f + std::sin(t * 2 * PI * 1810) * 0.3f) * (1 - p);
    });
    g_sounds[SFX_KO] = synth(0.9f, [](float t, float p) {
        return (noise() * 0.5f * (1 - p) + std::sin(t * 2 * PI * (90 - 50 * p)) * 0.9f) * (1 - p);
    });
    g_sounds[SFX_WHOOSH] = synth(0.15f, [](float, float p) { return noise() * 0.25f * std::sin(p * PI); });
    g_sounds[SFX_SELECT] = synth(0.05f, [](float t, float p) { return sq(t * 2 * PI * 660) * 0.2f * (1 - p); });
    g_sounds[SFX_CONFIRM] = synth(0.18f, [](float t, float p) {
        return sq(t * 2 * PI * (p < 0.5f ? 660 : 990)) * 0.22f * (1 - p);
    });
    g_ready = true;
}

void audioShutdown() {
    if (!g_ready) return;
    for (auto& s : g_sounds) UnloadSound(s);
    CloseAudioDevice();
    g_ready = false;
}

void playSfx(Sfx s) {
    if (g_ready) PlaySound(g_sounds[s]);
}

} // namespace client
