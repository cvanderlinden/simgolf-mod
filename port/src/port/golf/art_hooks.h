// Art pack hooks (task-17), see art_hooks.c. A listener (native: hd_art.c)
// fills the function pointers; NULL ones cost nothing.

#ifndef SIMGOLF_PORT_GOLF_ART_HOOKS_H
#define SIMGOLF_PORT_GOLF_ART_HOOKS_H

typedef struct {
  // FUN_00404120 opened FLC `path` into flic slot `slot`.
  void (*flic_open)(const char *path, int slot);
  // FUN_00473bf0 cut (x, y, w, h) of surface wrapper `src` into sprite wrapper `wrap`.
  void (*cut)(void *wrap, int src, int x, int y, int w, int h);
  // FUN_00475840 is loading PCX `path` into surface wrapper `surface`.
  void (*pcx_load)(void *surface, const char *path);
} ArtHooks;

extern ArtHooks art_hooks;

#endif  // SIMGOLF_PORT_GOLF_ART_HOOKS_H
