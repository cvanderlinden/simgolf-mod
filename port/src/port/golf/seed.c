// task-38: golf's random numbers can start from a fixed seed (SIMGOLF_SEED,
// port/src/port/runtime.c port_seed) so test scripts get the same new-game
// site on every run. Without it this is the original: seeded from the clock.

#include "golf_protos.h"
#include "port/runtime.h"

// @override FUN_00467110
// WinMain's one-time seeding: the seed at 0x51b380, the generator state at
// 0x822d9c (seed * 37).
DWORD FUN_00467110(void) {
  PORT_TRACE(0x00467110);
  DWORD seed = (DWORD)port_seed((uint32_t)timeGetTime());
  *(DWORD *)(uintptr_t)0x51b380 = seed;
  *(int *)(uintptr_t)0x822d9c = (int)(seed * 0x25);
  return seed;
}
