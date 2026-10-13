// The Options page, game-thread hooks (simgolf mod, card task-24).
//
// The wrench menu's item "Edit Preferences" (FUN_00432720 case 5, patched in tools/translate/golf.json) now
// opens the native Options page (options_frosted.c) instead of the original's list of six checks; those six
// stay available as the page's Gameplay rows. mods.ini [ui] options=original gives the original list back.
// The browser build has no native page: both hooks keep the original behaviour.

// an undeclared function returning a double leaves the x87 stack unbalanced: never allow it (the build passes -w)
#pragma clang diagnostic error "-Wimplicit-function-declaration"

#include "golf_protos.h"
#include "port/runtime.h"

#ifdef __EMSCRIPTEN__

void opt_game_frame(void) {}
void dbg_overlay_frame(void) {}
void opt_edit_prefs(void) { FUN_00432560(); }

#else

#include "port/options_frosted.h"

void opt_edit_prefs(void) {
  if (opt_native_on()) opt_open();
  else FUN_00432560();
}

#endif
