// The Mods screen, presenter side (simgolf mod, card task-22). Native build only.
//
// The page itself is drawn by ui/ui_mods.c over the model in mods/modmgr.h. This file owns the pieces
// that need the running game: the model (opened from the folder the game loaded its mods from, with the
// ids it loaded as the "running" set), the icon textures, the key queue, opening a mod's folder or link,
// and the layout log for the hidden test run. It has no layer of its own: the Options layer
// (options_frosted.c) is the presenter's top layer and hands this page its frames and its input, so the
// start menu's "Mods" pill, the Options page's "Mods" button and Shift+F12 all open it the same way.
#pragma once

#include <SDL.h>

#include "port/ui/ui_kit.h"

#ifdef __cplusplus
extern "C" {
#endif

// any thread
void modsp_open(void);
void modsp_close(void);
int modsp_is_open(void);

// render thread, inside ui_begin / ui_end of the Options layer
void modsp_draw(UiCtx *c, SDL_Renderer *r, int w, int h, double now, int in_game);
// game thread: a key press while the page is open (queued for the render thread). 1 = taken.
int modsp_key(const SDL_Event *e);

#ifdef __cplusplus
}
#endif
