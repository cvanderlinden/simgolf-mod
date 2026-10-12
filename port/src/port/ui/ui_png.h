// Frosted Mint UI kit (task-26): PNG writer (RGB, from ARGB8888 pixels).
#pragma once
#include <stdint.h>
// Returns 1 on success. pitch_px: pixels per row in argb.
int ui_png_write(const char *path, const uint32_t *argb, int w, int h, int pitch_px);
