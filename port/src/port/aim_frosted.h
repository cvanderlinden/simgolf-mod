// Frosted Mint shot aim arrows, render side (simgolf mod, task-115). See ui/ui_aim.h and golf/aim_hooks.c.
// Switch: ui.aim = frosted_mint | original (mods store; the built-in mod ui_aim sets frosted_mint).
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int aimf_enabled(void);   // game thread: ui.aim=frosted_mint and the world layer is up (registers it on first call)
#ifdef __cplusplus
}
#endif
