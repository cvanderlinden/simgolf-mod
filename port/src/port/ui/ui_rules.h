// ui_rules.h: small pure rules the HUD copies from golf.exe, header-only so tests/ui/test_ui.c can check them.
#ifndef UI_RULES_H
#define UI_RULES_H

// Is the building lot with the 0-based id `i` (tool id i + 6) clickable? The original draw and click
// (gen golf_02.c FUN_00434140 / its tray draw at 6064): the lot is open (id < unlock), not fully upgraded
// (level < 2), and an upgrade (level 1) also needs more than 10 holes. Card task-93: the HUD used
// "unlock - level", which greyed a built lot at the edge of the unlock range.
static inline int ui_lot_enabled(int i, int unlock, int level, int next_hole) {
  return i + 6 < unlock && level < 2 && (level < 1 || next_hole > 10);
}

#endif
