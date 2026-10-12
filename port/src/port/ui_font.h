// UI font switch (card task-25): the settings and tables behind the modern
// text path in gdi.c. ui.font=original keeps the game's own faces drawn 1-bit
// (128 threshold); ui.font=plus_jakarta draws Plus Jakarta Sans anti-aliased.
// Native build only (the mods store lives there); the browser build always
// reports the original.
#ifndef SIMGOLF_UI_FONT_H
#define SIMGOLF_UI_FONT_H

#include <stdint.h>

#ifdef __EMSCRIPTEN__
static inline int uif_enabled(void) { return 0; }
static inline int uif_load_map(void) { return 0; }
static inline int uif_style(const char *face, int lf_weight, int *weight) { (void)face; (void)lf_weight; (void)weight; return 0; }
static inline float uif_fit(void) { return 1.0f; }
static inline int uif_tabular(void) { return 0; }
static inline uint8_t uif_blend_chan(int dst, int src, int cov) { (void)dst; (void)cov; return (uint8_t)src; }
#else
#ifdef __cplusplus
extern "C" {
#endif
// 1 when the setting "ui.font" is "plus_jakarta" (re-read whenever any mods
// store value changes, so the bridge's mods_set switches it live).
int uif_enabled(void);
// Reads fonts/plus-jakarta-sans/font_map.txt (next to the exe, copied from
// data/ui/font_map.txt) once. Returns 1 when usable.
int uif_load_map(void);
// The Plus Jakarta Sans weight (400..800) for a game face and its requested
// weight; returns 0 when the map has no usable rule.
int uif_style(const char *face, int lf_weight, int *weight);
// Width fit: Plus Jakarta text is sized so a sample sentence is this fraction
// of the original face's width (so text fits the boxes drawn for the original).
float uif_fit(void);
int uif_tabular(void);   // digits on the tabular (tnum) glyphs
// One colour channel blended with `cov` (0..255) in linear light (gamma 1.8).
uint8_t uif_blend_chan(int dst, int src, int cov);
#ifdef __cplusplus
}
#endif
#endif
#endif
