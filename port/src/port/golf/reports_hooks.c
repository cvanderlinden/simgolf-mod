// Frosted Mint reports, hooks for the browser build (simgolf mod, task-28). The native build's hooks are
// in port/reports_game.c; the translator patches (tools/translate/golf.reports.json) call these. In the
// browser build each one keeps the original behaviour: the text calls draw as before, the rest do nothing.
#include "golf_protos.h"
#include "port/runtime.h"

#ifdef __EMSCRIPTEN__

void rp_tick(void) {}
void rp_begin(int kind) { (void)kind; }
void rp_frame(void) {}
void rp_done(void) {}
void rp_quad(void *w, unsigned a, unsigned b, unsigned c, unsigned d) { FUN_00476310(w, a, b, c, d); }
void rp_t650(void *w, int font, char *s, int x, int y, int len) { FUN_00476650(w, font, s, x, y, len); }
void rp_t6a0(void *w, int font, char *s, int x, int y, int wd, char *len) { FUN_004766a0(w, font, s, x, y, wd, len); }
void rp_t700(void *w, int font, char *s, int x, int y, char *len) { FUN_00476700(w, font, s, x, y, len); }
void rp_t750(void *w, int font, char *s, int x, int y, char *len) { FUN_00476750(w, font, s, x, y, len); }
int rp_tc(int fn, char *s, int x, int y, unsigned col) {
  switch (fn) {
    case 0: return FUN_00404b70(s, x, y, col);
    case 1: return FUN_00404bc0(s, x, y, col);
    case 2: return FUN_004049d0(s, x, y, col);
    default: return FUN_00404ad0(s, x, y, col);
  }
}
void rp_line(void *w, int x0, int y0, int x1, int y1, unsigned col) { FUN_00478b80(w, x0, y0, x1, y1, col); }
void rp_rect(void *page, int x, int y, int w, int h, unsigned col) { FUN_00480b00(page, x, y, w, h, col); }
int rp_chip(void *sprite, int target, int x, int y, int clip) { return FUN_00473e60(sprite, target, x, y, clip); }
int rp_ds(void *w, char *s, int x, int y, char *len) { return FUN_00478140(w, s, x, y, len); }
void rp_ov(int kind, int phase) { (void)kind, (void)phase; }

#endif
