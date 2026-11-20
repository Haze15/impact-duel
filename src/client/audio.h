#pragma once

namespace client {

enum Sfx { SFX_HIT, SFX_HEAVY, SFX_BLOCK, SFX_CLASH, SFX_KO, SFX_WHOOSH, SFX_SELECT, SFX_CONFIRM, SFX_COUNT };

void audioInit();
void audioShutdown();
void playSfx(Sfx s);

} // namespace client
