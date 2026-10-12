// Render-thread half of the round HD ball (task-41), see hdball.h.
#ifndef SIMGOLF_PORT_HDBALL_RENDER_H
#define SIMGOLF_PORT_HDBALL_RENDER_H

#include <SDL.h>

#include "port/golf/hd_ui.h"
#include "port/hdball.h"

// A new game frame reached the presenter: its ball record and the camera it
// was drawn with. course_layered: the frame is a layered course frame (else
// all history is dropped).
void hdball_ingest(const HdBallFrame *f, const HdUiCamera *cam, int course_layered, double now_ms);

// Draws every ball over the course layer. (cdx, cdy): where canvas pixel
// (0, 0) is on the target (slide included); k: target px per canvas px;
// clip: the visible course area on the target.
void hdball_draw(SDL_Renderer *r, int cdx, int cdy, const SDL_Rect *clip, float k, int canvas_w, int canvas_h,
                 double now_ms);

#endif
