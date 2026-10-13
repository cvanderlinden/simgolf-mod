// Frosted Mint UI kit (task-26): hit testing and input routing.
// Plain C, no SDL: unit tested in tests/ui/test_ui.c.
//
// Each frame the widgets add their regions to a UiHitList (later = on top).
// The finished list is published; input that arrives before the next frame
// is routed against it (ui_route): pointer events over a region, or that
// belong to a press that started on one, go to the UI and never reach the
// game; everything else goes to the game unchanged. The UI side turns the
// events it gets into hot / active / clicked ids (UiInteract), immediate mode.
#pragma once

#include <stdint.h>

#include "port/ui/ui_layout.h"

enum {
  UI_HIT_BLOCK = 1,       // swallows pointer input (panels, every control)
  UI_HIT_INTERACT = 2,    // can become hot / active / clicked (controls)
  UI_HIT_MODAL = 4,       // while present, every pointer and key event goes to the UI
  UI_HIT_WHEEL = 8,       // takes the mouse wheel (scrolling rows)
};

typedef struct {
  UiRect r;
  float radius;
  uint32_t id;      // 0 for plain blocking regions (panel bodies)
  uint32_t flags;
} UiHitRegion;

#define UI_HIT_MAX 512
typedef struct {
  UiHitRegion v[UI_HIT_MAX];
  int n;
  float clip_x0, clip_y0, clip_x1, clip_y1;   // regions are clipped to this (scrolling rows)
  int clipped;
} UiHitList;

void ui_hits_clear(UiHitList *h);
// Adds a region (ignored when full or empty); returns its index or -1.
int ui_hits_add(UiHitList *h, UiRect r, float radius, uint32_t id, uint32_t flags);
void ui_hits_clip(UiHitList *h, UiRect clip);   // later regions are cut to clip
void ui_hits_unclip(UiHitList *h);
// The topmost region under (x, y) that has any of the flags in mask, or -1.
int ui_hits_test(const UiHitList *h, float x, float y, uint32_t mask);
int ui_hits_modal(const UiHitList *h);

// ------------------------------------------------------------------ events

typedef enum {
  UI_EV_NONE, UI_EV_MOVE, UI_EV_DOWN, UI_EV_UP, UI_EV_WHEEL, UI_EV_KEY_DOWN, UI_EV_KEY_UP, UI_EV_TEXT, UI_EV_LEAVE
} UiEvKind;

typedef struct {
  UiEvKind kind;
  float x, y;      // pointer, target pixels (DOWN, UP, MOVE, WHEEL)
  int button;      // 1 left, 2 middle, 3 right (SDL numbering)
  float wheel;     // notches, + away from the user
  int key;         // SDL keycode (KEY_*), first byte of text for TEXT
  int mods;
} UiEvent;

// A bounded event queue that never loses a press or release (task-114l): when it is full, an incoming MOVE
// replaces the newest queued MOVE (or is dropped), and any other event pushes out the oldest MOVE or WHEEL first.
// Returns 1 when the event was queued.
int ui_evq_push(UiEvent *q, int *n, int cap, const UiEvent *e);

// ------------------------------------------------------------------ routing

enum { UI_ROUTE_GAME = 1, UI_ROUTE_UI = 2, UI_ROUTE_BOTH = 3 };

typedef struct {
  UiHitList published;    // the last finished frame's regions
  uint32_t ui_buttons;    // buttons pressed on the UI (their release goes to the UI too)
  uint32_t game_buttons;  // buttons pressed on the game
  int keyboard;           // the UI has keyboard focus (text field)
  float x, y;
  int over;               // the pointer is over a blocking region
} UiRouter;

void ui_router_init(UiRouter *r);
// Where an event goes. Pointer moves always reach the UI (hover); they reach
// the game too unless the pointer is over the UI or a UI press is held.
int ui_route(UiRouter *r, const UiEvent *e);

// ------------------------------------------------------------------ interaction

#define UI_CLICKS_MAX 8
typedef struct {
  float x, y;
  int inside;              // pointer inside the target
  uint32_t hot;            // control under the pointer
  uint32_t active;         // control the left button went down on
  uint32_t clicked[UI_CLICKS_MAX];   // released over the control it went down on, this frame
  int nclicked;
  uint32_t right_clicked;
  uint32_t pressed;        // went down this frame
  float wheel;             // wheel notches this frame
  uint32_t wheel_id;       // the wheel region under the pointer
  double hot_since;        // when hot last changed (tooltips)
  int down;                // left button held
} UiInteract;

void ui_interact_init(UiInteract *s);
// Feeds one frame of events against the regions the previous frame drew
// (the ones the user saw when acting). now is in ms.
void ui_interact_frame(UiInteract *s, const UiHitList *prev, const UiEvent *ev, int n, double now);
int ui_interact_clicked(const UiInteract *s, uint32_t id);

// ------------------------------------------------------------------ ids

uint32_t ui_id(const char *s);                 // FNV-1a, never 0
uint32_t ui_idi(uint32_t base, int i);         // derived id for item i
