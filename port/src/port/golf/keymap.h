// Key bindings (simgolf mod, card task-24, Options > Controls).
//
// Every keyboard action of the game (the camera, the build tools, the hole hotkeys, the reports, the game's own
// commands) has two key slots, a primary and a secondary, each a key with optional Ctrl, Shift and Alt:
// "W", "Ctrl+S", "Shift+1". The bindings live in the mods store as keys.<action> = "primary | secondary" ("-" is
// an empty slot, "default" the profile's own keys), so options.ini keeps them between runs.
//
// win32.c's event pump hands each key event to keymap_event() before the camera and the game see it. A key bound
// to an action is turned into what the game itself reads for that action (the character or virtual key the
// original key would have produced, or a camera pan); the physical key's own text is swallowed. A key that is
// the original key of an action that now has other keys is swallowed too, so a rebound action stops answering
// to its old key. Keys that no action uses (typing a name, Shift and Ctrl as the game polls them) pass
// untouched, and so does a key that is exactly the original key of the action it is bound to: with the default
// bindings of the original profile the game sees what it saw before, byte for byte. Only while the course view
// is in charge (camera_input_active): dialogs, menus and text entry get every key as typed.
//
// Two profiles give the defaults: "modern" (the camera pans on W A S D, so water, sand and draw moved to Q, C
// and K) and "original" (camera.wasd=0: the 2002 keys, no pan keys). camera.wasd in golf.ini [camera] or the store
// picks one; Options > Controls > Original keys / Modern keys sets it and clears every override.
#pragma once

#include <SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { KM_SLOTS = 2 };

// The pump: 1 when the event was taken.
int keymap_event(SDL_Event *e);

// The table, for the Options page (any thread).
int km_count(void);
const char *km_id(int a);
const char *km_label(int a);
const char *km_group(int a);
// The binding in slot 0 or 1 as text ("W", "Ctrl+S"); "" when empty.
void km_binding(int a, int slot, char *out, int n);
// Puts a binding in a slot ("" empties it): kept in the store and options.ini. 1 on success.
int km_set(int a, int slot, const char *combo);
// The action (and slot in *slot) that uses combo, other than action `except`; -1 when free.
int km_find(const char *combo, int except, int *slot);
// The combo a key event stands for ("Ctrl+Shift+W"); "" for a modifier alone or an event that is no key press.
int km_event_combo(const SDL_Event *e, char *out, int n);
// 1 when the key event is bound to the action with this id (used by the Options page's own keys).
int km_event_is(const SDL_Event *e, const char *id);
// profile: 1 modern, 0 original: sets camera.wasd and every action back to that profile's keys.
void km_reset(int modern);
// 1 when the active profile is the modern one.
int km_modern(void);

#ifdef __cplusplus
}
#endif
