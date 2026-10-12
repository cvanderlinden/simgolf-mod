// The Win32 subset that golf.exe and jgld.dll call, implemented over SDL2,
// the Emscripten FS and JSPI. The API functions themselves are defined with
// their Win32 names (win32.c, gdi.c) and plain 32-bit argument types; this
// header is what the rest of the port shares.

#ifndef SIMGOLF_PORT_WIN32_H
#define SIMGOLF_PORT_WIN32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t nu;

#define WIN_HWND 0x00010010u          // the one window
#define WIN_HINSTANCE 0x00400000u
#define WIN_JGLD_BASE 0x10000000u
// The game's screen ("canvas"). 800x600 in the original; the native build's
// display settings can make it wider or bigger (task-3, hd_native.c) before
// the window DC exists. golf.exe reads it back through GetSystemMetrics and
// the patched FUN_0045baf0 (tools/translate/golf.json).
extern int win_screen_w, win_screen_h;
extern int win_tier_w, win_tier_h;   // golf's art scale: 800x600 unless the canvas is 1024x768 or 1280x1024
#define WIN_SCREEN_W win_screen_w
#define WIN_SCREEN_H win_screen_h
void win_set_screen_size(int w, int h);

void win32_init(void);

// Logs the first calls to each Win32 shim when Module.apiTrace is set.
void api_trace(const char *name);
#define API_TRACE() api_trace(__func__)

// Handle table shared by kernel and GDI objects.
enum ObjKind {
  OBJ_FREE, OBJ_DC, OBJ_BITMAP, OBJ_FONT, OBJ_PALETTE, OBJ_RGN, OBJ_BRUSH,
  OBJ_FILE, OBJ_FIND, OBJ_MAPPING, OBJ_ICON, OBJ_EVENT
};
nu obj_new(int kind, void *data);
void *obj_get(nu h, int kind);
void obj_free(nu h);
int obj_kind(nu h);

// Window messages (win32.c).
void win_post(nu msg, nu wparam, nu lparam);
void win_invalidate(void);
// Present the window framebuffer (RGB555, 800x600) and maybe yield.
uint16_t *win_screen(void);
void win_present(void);
// Let the browser run: JSPI suspension of the game's call stack.
void win_yield(int ms);
uint32_t win_ticks(void);
// 1 once the game has destroyed its window or received WM_QUIT.
extern int win_closing;

// GDI (gdi.c)
void gdi_init(void);
nu gdi_window_dc(void);

#ifdef __cplusplus
}
#endif
#endif  // SIMGOLF_PORT_WIN32_H
