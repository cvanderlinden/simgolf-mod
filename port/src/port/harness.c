// Scripted runs for tests and evidence (native build, simgolf mod task-2):
// golf.exe --script file.txt runs one command per line from the game loop's
// yield points. Input goes in as SDL events, the same path real mouse and
// keyboard input takes (win32.c pump_sdl); screenshots come from the window's
// own 800x600 back buffer, so they work on a hidden desktop.
//
//   wait <ms>                  real time before the next command
//   move <x> <y>               mouse to a window position
//   click <x> <y> [left|right] [ms]  move, press 200 ms later, release after
//                              ms (default 100)
//   key <name> [n]             SDL key name (Return, Escape, Backspace, F5, a), n times
//   text <string>              typed characters
//   press <x> <y> [button]     move, then hold a button (left, right, middle)
//   release <x> <y> [button]   move, then let it go
//   drag <x0> <y0> <x1> <y1> [button] [steps]  press, move in steps 30 ms apart, release
//   wheel <x> <y> <n>          move, then n wheel notches (negative = towards you)
//   hold <key> <ms>            key down, held for ms, key up
//   type <string>              key down, character, key up per letter (as a keyboard)
//   shot <file.bmp>            the current frame, 24-bit BMP
//   hdshot <file.bmp>          the picture on screen at its full size (task-3,
//                              hd_native.c); the 800x600 frame when that's off
//   hddump <file>|off          from here on the course part of each presented
//                              frame, small, to <file> (task-68, hd_native.c)
//   wclick <x> <y> [right]     click at a point of the window as a real mouse
//   wmove <x> <y>              would (window pixels, mapped by hd_native.c)
//   windowmouse on|off         from here on move/click/press/drag/wheel go in
//                              as real window events: frame points are turned
//                              into the window pixels hd_native.c maps back
//   waitfile <text> <ms>       until the game opens a file whose path contains
//                              <text> (any case); fails the run after <ms>
//   pixel <x> <y> <rrggbb> [tol] the frame has that colour there (+-tol, default 24)
//   log <text>                 a line in golf.log
//   msg <subject> <text>       post a news message as the game does (task-62): subject -1 none,
//                              a golfer slot, or g = the first golfer on the course
//   pick <x> <y>               logs the tile golf's screenToWorld finds at frame
//                              point x,y, the terrain types around it and the
//                              camera (golf/hd_ui.c, task-37)
//   quit [code]                exit (default 0)
//
// A failed command exits with code 2 and says why in golf.log.

#include <SDL.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port/host.h"
#include "port/win32.h"

typedef struct { char op[16]; char arg[260]; int x, y, n; unsigned rgb; } Cmd;

int hd_native_shot(const char *name);   // hd_native.c (task-3)
int hd_native_dump(const char *path);   // hd_native.c (task-68)
int hd_native_unmap(int *x, int *y);    // frame point -> window pixel (task-15b)
void hd_ui_pick_log(int x, int y);      // golf/hd_ui.c (task-37)
unsigned FUN_0040cb00(unsigned style, int force, unsigned subject);   // golf: post a news message (text in text_buf 0x51a068)

static Cmd *g_cmds;
static int g_ncmds, g_pc = -1;
static double g_next;        // time the current command may run
static double g_wait_until;  // waitfile deadline
static char g_wait_text[260];
static int g_wait_hit;

static void add(const char *op, const char *arg, int x, int y, int n) {
  g_cmds = realloc(g_cmds, (g_ncmds + 1) * sizeof *g_cmds);
  Cmd *c = &g_cmds[g_ncmds++];
  memset(c, 0, sizeof *c);
  snprintf(c->op, sizeof c->op, "%s", op);
  snprintf(c->arg, sizeof c->arg, "%s", arg ? arg : "");
  c->x = x; c->y = y; c->n = n;
}

void harness_load(const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "harness: cannot read %s\n", path);
    host_exit(2);
  }
  char line[512];
  while (fgets(line, sizeof line, f)) {
    char *s = line;
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    if (!*s || *s == '#') continue;
    char op[16] = "", rest[400] = "";
    sscanf(s, "%15s %399[^\n]", op, rest);
    int x = 0, y = 0;
    if (!strcmp(op, "click") || !strcmp(op, "wclick")) {
      char b[16] = "";
      int hold = 100;   // task-38: an optional hold time, e.g. 50 for a quick click
      sscanf(rest, "%d %d %15s %d", &x, &y, b, &hold);
      int right = !strcmp(b, "right"), w = op[0] == 'w';
      add(w ? "wmove" : "move", NULL, x, y, 0);
      add("wait", NULL, 0, 0, 200);   // a hover frame first, as a real mouse gives
      add(w ? "wdown" : "down", NULL, x, y, right);
      add("wait", NULL, 0, 0, hold);
      add(w ? "wup" : "up", NULL, x, y, right);
      add("wait", NULL, 0, 0, 50);
    } else if (!strcmp(op, "press") || !strcmp(op, "release") || !strcmp(op, "drag")) {
      // task-15 camera tests: buttons held across moves
      int x1 = 0, y1 = 0, steps = 8;
      char b[16] = "left";
      if (!strcmp(op, "drag")) sscanf(rest, "%d %d %d %d %15s %d", &x, &y, &x1, &y1, b, &steps);
      else sscanf(rest, "%d %d %15s", &x, &y, b);
      int btn = !strcmp(b, "middle") ? 2 : !strcmp(b, "right") ? 1 : 0;
      add("move", NULL, x, y, 0);
      add("wait", NULL, 0, 0, 50);
      if (strcmp(op, "release")) add("down", NULL, x, y, btn);
      if (!strcmp(op, "drag")) {
        if (steps < 1) steps = 1;
        for (int k = 1; k <= steps; k++) {
          add("wait", NULL, 0, 0, 30);
          add("move", NULL, x + (x1 - x) * k / steps, y + (y1 - y) * k / steps, 0);
        }
        add("wait", NULL, 0, 0, 50);
        add("up", NULL, x1, y1, btn);
      } else if (!strcmp(op, "release")) add("up", NULL, x, y, btn);
      add("wait", NULL, 0, 0, 50);
    } else if (!strcmp(op, "wheel")) {
      int n = 0;
      sscanf(rest, "%d %d %d", &x, &y, &n);
      add("move", NULL, x, y, 0);
      add("wait", NULL, 0, 0, 50);
      add("wheel", NULL, x, y, n);
    } else if (!strcmp(op, "hold")) {
      char name[64] = "";
      int ms = 500;
      sscanf(rest, "%63s %d", name, &ms);
      add("keydown", name, 0, 0, 0);
      add("wait", NULL, 0, 0, ms);
      add("keyup", name, 0, 0, 0);
    } else if (!strcmp(op, "type")) {
      for (const char *p = rest; *p; p++) {
        char ch[2] = {*p, 0}, low[2] = {(char)tolower((unsigned char)*p), 0};
        int keyed = isalnum((unsigned char)*p);
        if (keyed) add("keydown", low, 0, 0, 0);
        add("text", ch, 0, 0, 0);
        add("wait", NULL, 0, 0, 40);
        if (keyed) add("keyup", low, 0, 0, 0);
        add("wait", NULL, 0, 0, 30);
      }
    } else if (!strcmp(op, "move") || !strcmp(op, "wmove") || !strcmp(op, "pick")) {
      sscanf(rest, "%d %d", &x, &y);
      add(op, NULL, x, y, 0);
    } else if (!strcmp(op, "wait")) {
      add("wait", NULL, 0, 0, atoi(rest));
    } else if (!strcmp(op, "waitfile")) {
      char text[260] = "";
      int ms = 30000;
      sscanf(rest, "%259s %d", text, &ms);
      add("waitfile", text, 0, 0, ms);
    } else if (!strcmp(op, "quit")) {
      add("quit", NULL, 0, 0, atoi(rest));
    } else if (!strcmp(op, "key")) {
      char name[64] = "";
      int count = 1;
      sscanf(rest, "%63s %d", name, &count);
      for (int k = 0; k < count; k++) {
        add("key", name, 0, 0, 0);
        add("wait", NULL, 0, 0, 30);
      }
    } else if (!strcmp(op, "text")) {
      // one character per 50 ms: the game's edit field takes one per frame
      for (const char *p = rest; *p; p++) {
        char ch[2] = {*p, 0};
        add("text", ch, 0, 0, 0);
        add("wait", NULL, 0, 0, 50);
      }
    } else if (!strcmp(op, "pixel")) {
      unsigned rgb = 0;
      int tol = 24;
      sscanf(rest, "%d %d %x %d", &x, &y, &rgb, &tol);
      add("pixel", NULL, x, y, tol);
      g_cmds[g_ncmds - 1].rgb = rgb;
    } else if (!strcmp(op, "windowmouse")) {
      add(op, NULL, 0, 0, !strcmp(rest, "on"));
    } else if (!strcmp(op, "msg")) {
      char subj[16] = "", text[300] = "";
      sscanf(rest, "%15s %299[^\n]", subj, text);
      add("msg", text, 0, 0, subj[0] == 'g' ? -2 : atoi(subj));
    } else if (!strcmp(op, "shot") || !strcmp(op, "hdshot") || !strcmp(op, "hddump") || !strcmp(op, "log")) {
      add(op, rest, 0, 0, 0);
    } else {
      fprintf(stderr, "harness: unknown command \"%s\"\n", s);
      host_exit(2);
    }
  }
  fclose(f);
  fprintf(stderr, "harness: %d steps from %s\n", g_ncmds, path);
  g_pc = 0;
}

// Files opened since the last input or screenshot command, so a waitfile right
// after "key Return" still sees a save that happened within that step.
static char g_recent[64][260];
static int g_nrecent;

static int matches(const char *path, const char *text) {
  size_t n = strlen(text);
  for (const char *p = path; *p; p++)
    if (!_strnicmp(p, text, n)) return 1;
  return 0;
}

void harness_note_file(const char *path) {
  if (g_pc < 0) return;
  if (g_nrecent < 64) snprintf(g_recent[g_nrecent++], sizeof g_recent[0], "%s", path);
  if (!g_wait_text[0] || g_wait_hit || !matches(path, g_wait_text)) return;
  g_wait_hit = 1;
  fprintf(stderr, "harness: %.1f s: opened %s\n", host_now_ms() / 1000.0, path);
}

// windowmouse on: the window's id and window pixels, as SDL's own events
static int g_window_mouse;

static Uint32 window_id(void) {
  for (Uint32 i = 1; i < 16; i++)
    if (SDL_GetWindowFromID(i)) return i;
  return 0;
}

static Uint32 to_window(int *x, int *y) {
  if (!g_window_mouse) return 0;
  hd_native_unmap(x, y);   // hd=0: the window is the frame
  return window_id();
}

static void push_motion(int x, int y) {
  SDL_Event e = {0};
  e.type = SDL_MOUSEMOTION;
  e.motion.windowID = to_window(&x, &y);
  e.motion.x = x;
  e.motion.y = y;
  SDL_PushEvent(&e);
}

static void push_button(int x, int y, int button, int down) {
  SDL_Event e = {0};
  e.button.windowID = to_window(&x, &y);
  e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.button = button == 2 ? SDL_BUTTON_MIDDLE : button ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
  e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.button.clicks = 1;
  e.button.x = x;
  e.button.y = y;
  SDL_PushEvent(&e);
}

// wclick: the event carries the window's id, as SDL's own mouse events do,
// so hd_native.c maps it from window pixels to the 800x600 frame.
static void push_window_mouse(const Cmd *c) {
  Uint32 id = 0;
  for (Uint32 i = 1; i < 16 && !id; i++)
    if (SDL_GetWindowFromID(i)) id = i;
  SDL_Event e = {0};
  if (!strcmp(c->op, "wmove")) {
    e.type = SDL_MOUSEMOTION;
    e.motion.windowID = id;
    e.motion.x = c->x;
    e.motion.y = c->y;
  } else {
    int down = !strcmp(c->op, "wdown");
    e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.windowID = id;
    e.button.button = c->n ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
    e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    e.button.clicks = 1;
    e.button.x = c->x;
    e.button.y = c->y;
  }
  SDL_PushEvent(&e);
}

// edges: 1 = down, 2 = up, 3 = both
static void push_key(const char *name, int edges) {
  SDL_Keycode k = SDL_GetKeyFromName(name);
  if (k == SDLK_UNKNOWN) {
    fprintf(stderr, "harness: unknown key %s\n", name);
    host_exit(2);
  }
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;
  e.key.state = SDL_PRESSED;
  e.key.keysym.sym = k;
  e.key.keysym.scancode = SDL_GetScancodeFromKey(k);
  if (edges & 1) SDL_PushEvent(&e);
  e.type = SDL_KEYUP;
  e.key.state = SDL_RELEASED;
  if (edges & 2) SDL_PushEvent(&e);
}

static void push_wheel(int n) {
  SDL_Event e = {0};
  e.type = SDL_MOUSEWHEEL;
  e.wheel.windowID = g_window_mouse ? window_id() : 0;
  e.wheel.y = n;
  SDL_PushEvent(&e);
}

static void push_text(const char *s) {
  for (; *s; s++) {
    SDL_Event e = {0};
    e.type = SDL_TEXTINPUT;
    e.text.text[0] = *s;
    SDL_PushEvent(&e);
  }
}

int harness_write_bmp(const char *path) {
  const uint16_t *px = win_screen();
  if (!px) return 0;
  FILE *f = fopen(path, "wb");
  if (!f) return 0;
  const int w = WIN_SCREEN_W, h = WIN_SCREEN_H, row = (w * 3 + 3) & ~3;   // BMP rows are 4-byte aligned
  uint8_t hdr[54] = {'B', 'M'};
  uint32_t size = 54 + (uint32_t)row * h;
  memcpy(hdr + 2, &size, 4);
  hdr[10] = 54; hdr[14] = 40;
  memcpy(hdr + 18, &w, 4);
  memcpy(hdr + 22, &h, 4);
  hdr[26] = 1; hdr[28] = 24;
  fwrite(hdr, 1, 54, f);
  uint8_t *line = calloc(1, row);
  for (int y = h - 1; y >= 0; y--) {
    for (int x = 0; x < w; x++) {
      uint16_t c = px[y * w + x];
      uint8_t r = (c >> 10) & 31, g = (c >> 5) & 31, b = c & 31;
      line[x * 3 + 0] = (uint8_t)(b << 3 | b >> 2);
      line[x * 3 + 1] = (uint8_t)(g << 3 | g >> 2);
      line[x * 3 + 2] = (uint8_t)(r << 3 | r >> 2);
    }
    fwrite(line, 1, row, f);
  }
  free(line);
  fclose(f);
  return 1;
}

void harness_tick(void) {
  static int busy;
  if (g_pc < 0 || busy) return;
  busy = 1;
  double now = host_now_ms();
  while (g_pc < g_ncmds && now >= g_next) {
    Cmd *c = &g_cmds[g_pc];
    if (!strcmp(c->op, "wait")) {
      g_next = now + c->n;
      g_pc++;
      break;
    }
    if (!strcmp(c->op, "waitfile")) {
      if (!g_wait_text[0]) {
        snprintf(g_wait_text, sizeof g_wait_text, "%s", c->arg);
        g_wait_hit = 0;
        g_wait_until = now + c->n;
        for (int k = 0; k < g_nrecent && !g_wait_hit; k++)
          if (matches(g_recent[k], c->arg)) {
            g_wait_hit = 1;
            fprintf(stderr, "harness: %.1f s: had opened %s\n", now / 1000.0, g_recent[k]);
          }
      }
      if (!g_wait_hit) {
        if (now > g_wait_until) {
          fprintf(stderr, "harness: FAIL no file matching \"%s\" opened within %d ms\n", c->arg, c->n);
          host_exit(2);
        }
        break;
      }
      g_wait_text[0] = 0;
      g_pc++;
      continue;
    }
    if (strcmp(c->op, "log")) g_nrecent = 0;   // waitfile looks from here on
    if (!strcmp(c->op, "hddump")) {
      const char *dir = getenv("SIMGOLF_SHOTS");
      char path[300];
      int abs = c->arg[0] == '/' || c->arg[0] == '\\' || (c->arg[0] && c->arg[1] == ':');
      if (dir && *dir && !abs) snprintf(path, sizeof path, "%s/%s", dir, c->arg);
      else snprintf(path, sizeof path, "%s", c->arg);
      hd_native_dump(strcmp(c->arg, "off") ? path : NULL);
      fprintf(stderr, "harness: %.1f s: hddump %s\n", now / 1000.0, c->arg);
      g_pc++;
      continue;
    }
    if (!strcmp(c->op, "hdshot")) {
      int hd = hd_native_shot(c->arg);   // 0: presentation off, take the frame
      if (hd < 0) {
        fprintf(stderr, "harness: %.1f s: hdshot %s FAILED\n", now / 1000.0, c->arg);
        host_exit(2);
      }
      if (hd) {
        g_pc++;
        continue;
      }
      strcpy(c->op, "shot");
    }
    if (!strcmp(c->op, "wmove") || !strcmp(c->op, "wdown") || !strcmp(c->op, "wup")) push_window_mouse(c);
    else if (!strcmp(c->op, "move")) push_motion(c->x, c->y);
    else if (!strcmp(c->op, "down")) push_button(c->x, c->y, c->n, 1);
    else if (!strcmp(c->op, "up")) push_button(c->x, c->y, c->n, 0);
    else if (!strcmp(c->op, "key")) push_key(c->arg, 3);
    else if (!strcmp(c->op, "keydown")) push_key(c->arg, 1);
    else if (!strcmp(c->op, "keyup")) push_key(c->arg, 2);
    else if (!strcmp(c->op, "wheel")) push_wheel(c->n);
    else if (!strcmp(c->op, "windowmouse")) {
      g_window_mouse = c->n;
      int x = 400, y = 300;
      to_window(&x, &y);
      fprintf(stderr, "harness: %.1f s: windowmouse %s (frame 400,300 is window %d,%d)\n", now / 1000.0,
              c->n ? "on" : "off", x, y);
    }
    else if (!strcmp(c->op, "text")) push_text(c->arg);
    else if (!strcmp(c->op, "pixel")) {
      const uint16_t *px = win_screen();
      uint16_t v = px[c->y * WIN_SCREEN_W + c->x];
      int r = (v >> 10) & 31, g = (v >> 5) & 31, b = v & 31;
      r = r << 3 | r >> 2; g = g << 3 | g >> 2; b = b << 3 | b >> 2;
      int er = (int)(c->rgb >> 16) & 255, eg = (int)(c->rgb >> 8) & 255, eb = (int)c->rgb & 255;
      int ok = abs(r - er) <= c->n && abs(g - eg) <= c->n && abs(b - eb) <= c->n;
      fprintf(stderr, "harness: %.1f s: pixel %d,%d is %02x%02x%02x, expected %06x: %s\n", now / 1000.0,
              c->x, c->y, r, g, b, c->rgb, ok ? "ok" : "FAIL");
      if (!ok) host_exit(2);
    } else if (!strcmp(c->op, "log")) fprintf(stderr, "harness: %.1f s: %s\n", now / 1000.0, c->arg);
    else if (!strcmp(c->op, "pick")) hd_ui_pick_log(c->x, c->y);
    else if (!strcmp(c->op, "msg")) {
      int subject = c->n;
      if (subject == -2) {   // the first golfer on the course (record 0x5794b8 + slot * 0x100, active byte at +0x21)
        subject = -1;
        for (int k = 0; k < 152; k++)
          if (*(volatile signed char *)(uintptr_t)(0x5794d9 + k * 0x100) && *(volatile signed char *)(uintptr_t)(0x5794d9 + k * 0x100) != -1) {
            subject = k;
            break;
          }
      }
      strcpy((char *)(uintptr_t)0x51a068, c->arg);
      unsigned ok = FUN_0040cb00(0x80004010u, 1, (unsigned)subject);
      fprintf(stderr, "harness: %.1f s: msg subject %d %s: %s\n", now / 1000.0, subject, ok ? "posted" : "REFUSED", c->arg);
    }
    else if (!strcmp(c->op, "shot")) {
      // relative names go to SIMGOLF_SHOTS (tools/ghost_run.ps1 -Shots)
      char path[600];
      const char *dir = getenv("SIMGOLF_SHOTS");
      int abs = c->arg[0] == '/' || c->arg[0] == '\\' || (c->arg[0] && c->arg[1] == ':');
      if (dir && *dir && !abs) snprintf(path, sizeof path, "%s/%s", dir, c->arg);
      else snprintf(path, sizeof path, "%s", c->arg);
      int ok = harness_write_bmp(path);
      fprintf(stderr, "harness: %.1f s: shot %s%s\n", now / 1000.0, path, ok ? "" : " FAILED");
      if (!ok) host_exit(2);
    } else if (!strcmp(c->op, "quit")) {
      fprintf(stderr, "harness: quit %d\n", c->n);
      host_exit(c->n);
    }
    g_pc++;
  }
  busy = 0;
}
