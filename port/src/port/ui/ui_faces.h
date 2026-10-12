// Frosted Mint UI kit (task-60): the golfer mood faces, drawn as vectors.
//
// Golf has ten mood faces (its sprite table at 0x59b778, index 1 worst to 10
// best, 0 the blank one the lists use for an empty slot; a golfer's mood plus 2
// clamped to 1..10 picks one). These are the same ten in the Frosted Mint style:
// a glassy round face, a soft light gradient from coral (bad) through amber to
// mint (good), a pale rim and a top gloss, features in deep green ink with round
// ends. Each face is rasterised at the exact pixel size it is drawn at (a signed
// distance field per shape, so edges are anti-aliased at any size), like the
// line icons. Original art, drawn in code (art/manifests/ui-mood-faces.json).
// Plain C: unit tested, no SDL.
#pragma once

#include <stdint.h>

#define UI_FACE_COUNT 11   // 0..10

// Rasterises mood face m (0..10, others are clamped) into size x size ARGB8888
// pixels (not premultiplied). Returns 1 on success.
int ui_face_raster(int mood, int size, uint32_t *argb);

// Rough mean colour of a face's disc (ARGB), for tests and the contact sheet captions.
uint32_t ui_face_colour(int mood);
