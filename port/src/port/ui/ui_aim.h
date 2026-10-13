// Frosted Mint shot aim arrows (simgolf mod, task-115). When the player's pro is out and a shot is chosen
// (Straight, Fade, Draw, High backspin, Low punch), golf draws the shot's flight over the course: a ground
// track in black and a chain of white segments through the points of the arc (FUN_0040f5c0, the block that
// writes the Attitude / Club / Distance / Lie readout). The hooks in golf/aim_hooks.c take those points as
// golf computes them, so the world positions are golf's own, and this draws them again as a smooth arrow:
// a curve with an arrowhead and a ring where the ball lands. docs/ui/KIT.md.
//
// Positions are canvas pixels of the frame as golf drew it (worldToScreen), pinned like the cart path
// view's (PvMap in ui_paths.h).
#pragma once

#include <SDL.h>
#include <stdint.h>

#include "port/ui/ui_kit.h"
#include "port/ui/ui_paths.h"

#include "port/aim_note.h"

// The arrow of one note. t_ms drives the small pulse of the ring.
void ui_aim_draw(UiCtx *c, const AimNote *n, const PvMap *m, double t_ms);
