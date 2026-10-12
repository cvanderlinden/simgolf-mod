// Round HD golf ball (simgolf mod, task-41, with the visual-only physics of
// task-43 option a). Setting ui.ball=round|original (mods store; the
// built-in mod ui_ball sets round; [ui] ball=original in data/mods.ini, the
// vanilla profile or --no-mods give the original ball pixel for pixel).
//
// Three parts:
//  - hdball.c, game thread: golf's own ball sprite and shadow are drawn as
//    usual into the course, then taken out again (their pixels restored to
//    what was under them) with a measure of how much of them later sprites
//    covered; the ball's position per game frame is recorded. Nothing the
//    game computes is touched: positions, timing and scores are golf's.
//  - hd_native.c hands the record over with the frame it belongs to.
//  - hdball_render.c, render thread: keeps the last few game-frame positions
//    of every ball and draws a shaded round ball with a soft ground shadow
//    at the display rate, on a smooth path through golf's own positions
//    (monotone cubic, so no overshoot: eased apex, roll that settles, never
//    backwards), one game frame behind golf, ending exactly where golf puts
//    the ball. Tuning numbers are in data/ui/ball.txt.
#ifndef SIMGOLF_PORT_HDBALL_H
#define SIMGOLF_PORT_HDBALL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HB_MAX 152   // golfer slots (one ball each)

enum { HB_KIND_REST = 0, HB_KIND_AIR = 1 };

typedef struct {
  int16_t slot;
  uint8_t kind;           // HB_KIND_*
  uint8_t trail;          // golf drew its trail segment for this ball this frame (task-61: the shots golf marks with a streak)
  int16_t person;         // the golfer's person id (1..75, record +0xb6): stable across saves, keys the personal ball colour (task-61)
  int16_t pad;
  int32_t wx, wy, wz;     // golf's own ball position (1/1024 tile, z the same unit)
  float gx, gy;           // the ground point on the canvas, as golf's worldToScreen gave it (terrain height included)
  float lift;             // golf's lift of the ball above that point, canvas px
  float occ_ball;         // 0..1: how much of golf's ball sprite later sprites left uncovered (1 when unknown)
  float occ_shadow;       // the same for golf's shadow sprite
} HdBallRec;

typedef struct {
  int on;                 // the round ball is drawn for this frame (and golf's own was taken out)
  int n;
  int zoom2;              // golf's 2x zoom view: the canvas shows the middle half at twice the size
  uint32_t frame;
  double t_ms;            // host_now_ms when the game thread finished the frame
  HdBallRec b[HB_MAX];
} HdBallFrame;

// ---- game thread (hdball.c)
int hdball_on(void);                                   // the round ball is wanted and can be shown
// ui.ball_trail (task-61): how the trail behind a ball in flight is drawn.
enum { HB_TRAIL_SMOOTH = 0, HB_TRAIL_ALL, HB_TRAIL_ORIGINAL, HB_TRAIL_OFF };
int hdball_trail_mode(void);
int hb_trail(int slot);                                // golf is about to draw its trail segment: 1 = skip it (the presenter draws the ribbon)
// Called by the hooks in golf/ball_hooks.c.
int hb_note(int slot, int kind, int sx, int sy);       // records the ball; 1 when the round ball is on this frame
int hb_frame_on(void);
int hb_match(int kind, int sx, int sort_y);            // record index for a queued sprite, -1 if none
void hb_snap_begin(int kind, int rec, int x0, int y0, int w, int h);
void hb_snap_end(void);
void hdball_resolve(void);                             // after golf drew its sprites
void hdball_publish(HdBallFrame *out);                 // hd_present: this frame's record, then a fresh one

// ---- native presenter (hd_native.c)
int hd_native_balls_ok(void);   // the layered presenter is running (the round ball needs the course layer)

#ifdef __cplusplus
}
#endif
#endif  // SIMGOLF_PORT_HDBALL_H
