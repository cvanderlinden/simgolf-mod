// Frosted Mint UI kit (task-26): drawing primitives over an SDL renderer.
//
// Everything is SDL_RenderGeometry, so it runs on the presenter's direct3d11
// renderer and on SDL's software renderer (the gallery and tests) alike.
// Shapes get a one-pixel alpha fringe on their outline (anti-aliased edges at
// any size); gradients are per-vertex colours (exact for linear gradients);
// soft shadows are rings sampled from a Gaussian edge; the frosted backdrop is
// the course layer scaled down in halves (each step a 2x2 box filter), smoothed
// twice, scaled back up in steps of four, then clipped to the panel shape.
#pragma once

#include <SDL.h>

#include "port/ui/ui_layout.h"
#include "port/ui/ui_tokens.h"

#define UID_BLUR_LEVELS 7

typedef struct UiDraw {
  SDL_Renderer *r;
  SDL_Texture *target;    // where the UI goes (NULL = the window)
  int w, h;               // its size
  SDL_Vertex *v;
  int *idx;
  int nv, ni, vcap, icap;
  // backdrop blur
  SDL_Texture *lv[UID_BLUR_LEVELS];
  int lw[UID_BLUR_LEVELS], lh[UID_BLUR_LEVELS];
  SDL_Texture *smooth[2];
  int sw, sh, sw1, sh1;
  SDL_Texture *full;      // the blur scaled back up to the target size
  int fw, fh;
  SDL_Texture *backdrop;  // blurred course for this frame, or NULL
  int clip_on;
  SDL_Rect clip;
} UiDraw;

void uid_init(UiDraw *d, SDL_Renderer *r);
void uid_free(UiDraw *d);
void uid_begin(UiDraw *d, SDL_Texture *target, int w, int h);

// Builds the blurred copy of src (w x h, the target size) for panels drawn
// after it. blur_px is the design blur in screen pixels. Returns 1 when ready.
int uid_backdrop(UiDraw *d, SDL_Texture *src, float blur_px);
void uid_no_backdrop(UiDraw *d);

void uid_rrect(UiDraw *d, UiRect r, float radius, UiColor c);
void uid_rrect_grad(UiDraw *d, UiRect r, float radius, UiGrad g);
// Two-colour vertical gradient (top to bottom), e.g. world.bg.
void uid_rect_vgrad3(UiDraw *d, UiRect r, UiColor top, UiColor mid, UiColor bottom);
// Border inside the outline (CSS border), width in pixels.
void uid_rrect_stroke(UiDraw *d, UiRect r, float radius, float width, UiColor c);
// The light line along the inside of the top edge (CSS inset 0 1px 0).
void uid_rrect_top_light(UiDraw *d, UiRect r, float radius, float width, UiColor c);
// Soft drop shadow, values already in pixels (x, y offset, blur).
void uid_rrect_shadow(UiDraw *d, UiRect r, float radius, float x, float y, float blur, UiColor c);
// The blurred backdrop inside the shape (no-op without uid_backdrop).
int uid_rrect_backdrop(UiDraw *d, UiRect r, float radius);
// A thick line with round caps (dividers, meters' ticks, arrows).
void uid_line(UiDraw *d, float x0, float y0, float x1, float y1, float width, UiColor c);
void uid_triangle(UiDraw *d, float x0, float y0, float x1, float y1, float x2, float y2, UiColor c);
// A texture (or part of it) into dst, colour-modulated; radius > 0 clips it to
// a rounded rectangle (round emblems and portraits).
void uid_image(UiDraw *d, SDL_Texture *tex, const SDL_Rect *src, UiRect dst, float radius, UiColor mod);
void uid_set_clip(UiDraw *d, const UiRect *r);   // NULL: none

UiColor uid_alpha(UiColor c, float a);           // c with its alpha times a
UiColor uid_mix(UiColor a, UiColor b, float t);
