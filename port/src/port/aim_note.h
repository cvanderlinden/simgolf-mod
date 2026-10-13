// The snapshot of golf's shot aim for the presenter (task-115). Plain data, no includes beyond stdint, so the
// game-side hooks (which include golf's own headers) can use it. See ui/ui_aim.h.
#pragma once
#include <stdint.h>

#define AIM_MAGIC 0x314d4941u   // "AIM1"
#define AIM_PTS 8

typedef struct AimNote {
  uint32_t magic;
  uint32_t serial;
  int32_t n;              // points of the flight arc (start first), 0 when no arrow is shown
  int32_t ng;             // points of the ground track
  int32_t shot;           // golf's shot choice: 0 straight, -1 fade, 1 draw, 3 high backspin, 4 low punch
  int32_t zoom;           // golf's zoom (1..8)
  int16_t ex[AIM_PTS], ey[AIM_PTS];   // the arc: the ball in the air (canvas px)
  int16_t gx[AIM_PTS], gy[AIM_PTS];   // the same points on the ground
} AimNote;

