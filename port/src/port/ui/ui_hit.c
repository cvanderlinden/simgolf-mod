// Frosted Mint UI kit: hit testing and input routing, see ui_hit.h.
#include "port/ui/ui_hit.h"

#include <string.h>

void ui_hits_clear(UiHitList *h) {
  h->n = 0;
  h->clipped = 0;
}

void ui_hits_clip(UiHitList *h, UiRect c) {
  h->clipped = 1;
  h->clip_x0 = c.x;
  h->clip_y0 = c.y;
  h->clip_x1 = c.x + c.w;
  h->clip_y1 = c.y + c.h;
}

void ui_hits_unclip(UiHitList *h) { h->clipped = 0; }

int ui_hits_add(UiHitList *h, UiRect r, float radius, uint32_t id, uint32_t flags) {
  if (h->clipped) {
    UiRect c = {h->clip_x0, h->clip_y0, h->clip_x1 - h->clip_x0, h->clip_y1 - h->clip_y0};
    UiRect k = ui_rect_intersect(r, c);
    // A control cut by the clip keeps its corners only where it is whole.
    if (k.w != r.w || k.h != r.h) radius = 0;
    r = k;
  }
  if (h->n >= UI_HIT_MAX || r.w <= 0 || r.h <= 0) return -1;
  UiHitRegion *g = &h->v[h->n];
  g->r = r;
  g->radius = radius;
  g->id = id;
  g->flags = flags;
  return h->n++;
}

int ui_hits_test(const UiHitList *h, float x, float y, uint32_t mask) {
  for (int i = h->n - 1; i >= 0; i--) {
    const UiHitRegion *g = &h->v[i];
    if ((g->flags & mask) && ui_rrect_contains(g->r, g->radius, x, y)) return i;
  }
  return -1;
}

int ui_hits_modal(const UiHitList *h) {
  for (int i = 0; i < h->n; i++)
    if (h->v[i].flags & UI_HIT_MODAL) return 1;
  return 0;
}

// ------------------------------------------------------------------ routing

void ui_router_init(UiRouter *r) {
  memset(r, 0, sizeof *r);
  r->x = r->y = -1e9f;
}

static uint32_t bit(int button) { return button >= 1 && button <= 31 ? 1u << button : 0; }

int ui_route(UiRouter *r, const UiEvent *e) {
  int modal = ui_hits_modal(&r->published);
  switch (e->kind) {
    case UI_EV_MOVE:
      r->x = e->x;
      r->y = e->y;
      r->over = ui_hits_test(&r->published, e->x, e->y, UI_HIT_BLOCK | UI_HIT_MODAL) >= 0;
      if (r->game_buttons) return UI_ROUTE_BOTH;   // a drag that began on the course stays the game's
      return modal || r->ui_buttons || r->over ? UI_ROUTE_UI : UI_ROUTE_BOTH;
    case UI_EV_DOWN: {
      r->x = e->x;
      r->y = e->y;
      r->over = ui_hits_test(&r->published, e->x, e->y, UI_HIT_BLOCK | UI_HIT_MODAL) >= 0;
      // A second button while one is held goes where the first one went.
      int to_ui = r->ui_buttons ? 1 : r->game_buttons ? 0 : (modal || r->over);
      if (to_ui) r->ui_buttons |= bit(e->button);
      else r->game_buttons |= bit(e->button);
      return to_ui ? UI_ROUTE_UI : UI_ROUTE_GAME;
    }
    case UI_EV_UP:
      r->x = e->x;
      r->y = e->y;
      if (r->ui_buttons & bit(e->button)) {
        r->ui_buttons &= ~bit(e->button);
        return UI_ROUTE_UI;
      }
      if (r->game_buttons & bit(e->button)) {
        r->game_buttons &= ~bit(e->button);
        return UI_ROUTE_GAME;
      }
      return modal || r->over ? UI_ROUTE_UI : UI_ROUTE_GAME;   // a release nobody saw go down
    case UI_EV_WHEEL:
      return modal || ui_hits_test(&r->published, r->x, r->y, UI_HIT_BLOCK | UI_HIT_MODAL) >= 0 ? UI_ROUTE_UI
                                                                                               : UI_ROUTE_GAME;
    case UI_EV_KEY_DOWN:
    case UI_EV_KEY_UP:
    case UI_EV_TEXT:
      return modal || r->keyboard ? UI_ROUTE_UI : UI_ROUTE_GAME;
    case UI_EV_LEAVE:
      r->over = 0;
      r->x = r->y = -1e9f;
      return UI_ROUTE_BOTH;
    default:
      return UI_ROUTE_GAME;
  }
}

// ------------------------------------------------------------------ interaction

void ui_interact_init(UiInteract *s) {
  memset(s, 0, sizeof *s);
  s->x = s->y = -1e9f;
}

static uint32_t hot_at(const UiHitList *h, float x, float y, int inside) {
  if (!inside) return 0;
  int i = ui_hits_test(h, x, y, UI_HIT_BLOCK | UI_HIT_INTERACT | UI_HIT_MODAL);
  return i >= 0 && (h->v[i].flags & UI_HIT_INTERACT) ? h->v[i].id : 0;
}

void ui_interact_frame(UiInteract *s, const UiHitList *prev, const UiEvent *ev, int n, double now) {
  s->nclicked = 0;
  s->right_clicked = 0;
  s->pressed = 0;
  s->wheel = 0;
  s->wheel_id = 0;
  for (int i = 0; i < n; i++) {
    const UiEvent *e = &ev[i];
    switch (e->kind) {
      case UI_EV_MOVE:
        s->x = e->x, s->y = e->y, s->inside = 1;
        break;
      case UI_EV_LEAVE:
        s->inside = 0;
        break;
      case UI_EV_DOWN:
        s->x = e->x, s->y = e->y, s->inside = 1;
        if (e->button == 1) {
          s->down = 1;
          s->active = hot_at(prev, s->x, s->y, 1);
          s->pressed = s->active;
        }
        break;
      case UI_EV_UP: {
        s->x = e->x, s->y = e->y;
        uint32_t h = hot_at(prev, s->x, s->y, 1);
        if (e->button == 1) {
          if (s->active && h == s->active && s->nclicked < UI_CLICKS_MAX) s->clicked[s->nclicked++] = s->active;
          s->active = 0;
          s->down = 0;
        } else if (e->button == 3 && h) {
          s->right_clicked = h;
        }
        break;
      }
      case UI_EV_WHEEL: {
        s->wheel += e->wheel;
        int k = ui_hits_test(prev, s->x, s->y, UI_HIT_WHEEL);
        s->wheel_id = k >= 0 ? prev->v[k].id : 0;
        break;
      }
      default:
        break;
    }
  }
  uint32_t h = hot_at(prev, s->x, s->y, s->inside);
  if (h != s->hot) {
    s->hot = h;
    s->hot_since = now;
  }
}

int ui_interact_clicked(const UiInteract *s, uint32_t id) {
  for (int i = 0; i < s->nclicked; i++)
    if (s->clicked[i] == id) return 1;
  return 0;
}

uint32_t ui_id(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) h = (h ^ (uint8_t)*s) * 16777619u;
  return h ? h : 1;
}

uint32_t ui_idi(uint32_t base, int i) {
  uint32_t h = (base ^ (uint32_t)(i + 1) * 0x9E3779B9u) * 16777619u;
  return h ? h : 1;
}
