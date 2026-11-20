#pragma once
#include "core/sim.h"

namespace client {

struct AIState {
    int level = 2;          // 1 facile, 2 normal, 3 difficile
    int think = 0;
    uint8_t plan = 0;       // directions maintenues
    uint8_t queuedPress = 0;
    int pressDelay = 0;
    uint32_t rng = 0x9E3779B9u;
};

fg::Input aiInput(const fg::Match& m, int me, AIState& s);

} // namespace client
