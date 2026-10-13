// Frosted Mint UI kit (task-26): rectangles, UI scale, anchors and stacks.
// Plain C, no SDL: unit tested in tests/ui/test_ui.c.
#pragma once

typedef struct { float x, y, w, h; } UiRect;

typedef enum {
  UI_TOP_LEFT, UI_TOP, UI_TOP_RIGHT,
  UI_LEFT, UI_CENTER, UI_RIGHT,
  UI_BOTTOM_LEFT, UI_BOTTOM, UI_BOTTOM_RIGHT
} UiAnchor;

// Default UI scale for a screen height (DESIGN_SYSTEM.md section 2):
// height / 1080 rounded to 0.25, never below 0.75 (the 800x600 row).
float ui_scale_for_height(int screen_h);

// A token size at a scale, rounded to whole pixels (crisp edges).
float ui_px(float scale, float v);
// A text size at a scale with the floor (design: never below 11 px).
float ui_text_px(float scale, float size, float floor_px);

static inline UiRect ui_rect(float x, float y, float w, float h) { UiRect r = {x, y, w, h}; return r; }
UiRect ui_inset(UiRect r, float l, float t, float right, float b);
UiRect ui_inset_all(UiRect r, float d);
UiRect ui_translate(UiRect r, float dx, float dy);
// A w x h rectangle placed at an anchor of parent, moved inwards by (dx, dy)
// (for UI_BOTTOM_RIGHT, dx moves left and dy up).
UiRect ui_anchor(UiRect parent, float w, float h, UiAnchor a, float dx, float dy);
UiRect ui_center(UiRect parent, float w, float h);
int ui_rect_contains(UiRect r, float x, float y);
// Point in a rounded rectangle (radius clamped to half the short side).
int ui_rrect_contains(UiRect r, float radius, float x, float y);
UiRect ui_rect_union(UiRect a, UiRect b);
UiRect ui_rect_intersect(UiRect a, UiRect b);   // w or h 0 when they do not meet
UiRect ui_rect_round(UiRect r);                // to whole pixels (edges rounded)

// Stacks: children one after another along an axis inside an area.
typedef struct {
  UiRect area;
  int horizontal;
  float gap, cursor;
  int count;
} UiStack;
UiStack ui_stack(UiRect area, int horizontal, float gap);
// The next child, size along the axis, full size across it.
UiRect ui_stack_next(UiStack *s, float size);
// The next child of size w x h, aligned across the axis (0 start, 0.5 centre, 1 end).
UiRect ui_stack_next_sized(UiStack *s, float w, float h, float align);
// Space left along the axis.
float ui_stack_left(const UiStack *s);

// Total length of n items of size each with gaps (row of tiles, dock).
float ui_row_length(int n, float size, float gap);

// Length in bytes of the longest prefix of s that is at most max bytes and does not end in the middle of a UTF-8
// character (task-114m: labels cut by a byte count left half a character, drawn as a box).
int ui_utf8_cut(const char *s, int max);
