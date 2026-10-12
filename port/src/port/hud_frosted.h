// Frosted Mint course HUD (simgolf mod, task-30 on the task-26 UI kit).
//
// Two halves that meet here:
//   hud_game.c         game thread: hides the old HUD (translator patches in
//                      FUN_0040f5c0; golf/hud_hooks.c has the overview, the
//                      news strip override and the browser build's no-ops),
//                      reads the game's state once per course frame into a
//                      HudSnap, snapshots the game's own tool art, and turns
//                      HUD actions into clicks on the ORIGINAL wheel and tray
//                      controls (the game's own handlers run).
//   hud_frosted.c      render thread: draws the HUD with the kit (port/ui) as a
//                      native UI layer of the presenter (hd_native.h,
//                      hd_ui_add_layer / hd_ui_add_input) and routes the pointer.
// Switch: ui.hud = frosted_mint | original (mods store; the built-in mod
// ui_hud sets frosted_mint). docs/ui/KIT.md, docs/ui/HUD_DISPATCH.md.
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUD_ART_MAX 64

enum {   // art slots (game sprites read back with alpha)
  HUD_ART_TERRAIN = 0,      // 16 terrain tray slots
  HUD_ART_AMENITY = 16,     // 8 amenities
  HUD_ART_BUILDING = 24,    // 9 building lots
  HUD_ART_ELEVATION = 33,   // 4 elevation tools
  HUD_ART_EMBLEM = 37,      // the course emblem
};

// One game message (task-62): what the game's news strip said, kept for the HUD's history.
#define HUD_MSG_MAX 50
enum { HUD_SHOW_NONE = 0, HUD_SHOW_GOLFER = 1, HUD_SHOW_HOLE = 2 };
typedef struct HudMsg {
  char text[400];
  char date[24];     // the game's date when it came
  double t_ms;       // host clock when it came (the HUD says "2 min ago")
  int seq;           // 1, 2, 3 ... in order of arrival
  int show;          // HUD_SHOW_*: the game can show what it is about
  int target;        // golfer slot, or hole number 1..18
} HudMsg;

typedef struct HudSnap {
  int valid;
  uint32_t serial;
  int canvas_w, canvas_h;
  int visible;            // course view running, no dialog or info screen over it
  int capsule_visible;    // no golfer card / hole stats at the top
  int money;              // dollars
  int sandbox, cheat;
  int fun, skill_x100, golfers, waiting;
  char course[64], date[48];
  int dock_mode, amenities_on, elevation_on, employee_overlay;
  int terrain_tool, build_tool, elevation_tool;
  int paused, fast, zoom, rotation, theme, difficulty;
  int unlock, next_hole, building_level[9];
  int news_on;
  char news[400];
  uint32_t art_serial;    // bumps when hud_hooks re-read the art
  int elev_edit;          // flags & 2: an elevation tool is armed
  char terrain_name[16][24];
  int terrain_cost[16];   // dollars, -1 unknown
  char building_name[9][24];
  int building_cost[9];   // dollars for the next level, -1 unknown
  int people_mode;        // 2 golfers, 3/4 player, with employee_overlay
  int shape;              // the placement shape / rotation counter (Tab), 0x5a34f0
  int news_seq;           // task-62: seq of the message the news strip is showing now (0: none)
  int msg_n;              // history, oldest first, the last HUD_MSG_MAX messages
  HudMsg msgs[HUD_MSG_MAX];
} HudSnap;

enum { HUD_ACT_CLICK = 1, HUD_ACT_KEY = 2, HUD_ACT_VKEY = 3, HUD_ACT_GOAL = 4,
       HUD_ACT_SHOW = 5 };   // SHOW: x = HUD_SHOW_*, y = golfer slot or hole number (camera jump, as the game's own clicks and keys)
enum {   // goals: reached over a few frames, one original click per frame
  HUD_GOAL_COURSE = 1, HUD_GOAL_EXTRAS, HUD_GOAL_BUILDINGS, HUD_GOAL_LANDSCAPE, HUD_GOAL_PEOPLE, HUD_GOAL_CLOSE
};
typedef struct {
  int kind;
  int x, y;      // CLICK (and GOAL's follow-up click): a point of the original 800x600 layout;
                 // GOAL only: x == -1 no follow-up, x <= -2 terrain tray slot -2 - x
  int goal;      // GOAL: HUD_GOAL_*
  int key;       // KEY: character for the 0x822b9c mailbox; VKEY: VK code for 0x822b98
} HudAction;

// render side (hud_frosted.c), called from the game thread
int hudf_enabled(void);                       // ui.hud=frosted_mint and the native layer is up
void hudf_publish(const HudSnap *s);
int hudf_take_action(HudAction *a);           // next queued action, 1 if any
void hudf_publish_art(int slot, const uint32_t *argb, int w, int h);   // NULL clears
void hudf_set_injecting(int on);              // the game thread is feeding it a click

// game side (hud_game.c), for the render side and tests
int hud_injecting(void);

#ifdef __cplusplus
}
#endif
