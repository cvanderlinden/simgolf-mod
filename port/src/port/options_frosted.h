// The Options page (simgolf mod, card task-24; the Textures choice is card task-71).
//
// A native Frosted Mint page drawn by the presenter's top UI layer (hd_native.h
// hd_ui_set_top) over the front end and over the course: tabs (Graphics,
// Gameplay, Controls, Audio), every row a choice, switch or slider on one
// setting of the mods store (mods.h, "section.key"). A change takes effect
// now where the code reads the store while running, is kept in options.ini
// (mods_option_set) and is read again at the next start; rows that only
// apply at start say so. The page takes every pointer and key event while it
// is open, so the game underneath sees nothing, and in a running course the
// game is paused while it is up (opt_game_frame).
//
// Opened from the front end (the Options pill next to Quit), from the wrench menu (its "Edit Preferences" item
// became "Options...") and by F12 anywhere. Esc closes. F11 cycles the Textures look anywhere with a short note.
// mods.ini [ui] options=original gives the original preferences list back.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// any thread
void opt_open(void);
void opt_close(void);
int opt_is_open(void);
void opt_cycle_textures(void);   // HD -> Pixel HD -> Original -> HD, with a note on screen

// game thread
int opt_native_on(void);         // ui.options is not "original" and the native layer is up
void opt_game_frame(void);       // once per course frame: pauses the game while the page is open

// presenter start (hd_native.c), render thread
void opt_register(void);

#ifdef __cplusplus
}
#endif
