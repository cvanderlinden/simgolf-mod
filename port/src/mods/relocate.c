// Relocation helper (simgolf mod, task-11): moving a fixed-size game array to
// a bigger block, for the cards that raise the game's limits (bigger maps,
// more golfers). The translated game keeps 32-bit pointers at their original
// addresses, so pointers into an array are rebased by offset.

#include <stdlib.h>
#include <string.h>

#include "mods/mods_core.h"

void *mods_grow_array(const void *old, size_t elem, size_t old_n, size_t new_n) {
  if (!elem || new_n < old_n) return NULL;
  void *p = calloc(new_n ? new_n : 1, elem);
  if (p && old && old_n) memcpy(p, old, elem * old_n);
  return p;
}

static int rebase(uint32_t *ptr, uint32_t old_base, uint32_t old_size, uint32_t new_base) {
  uint32_t v = *ptr;
  if (v < old_base || v - old_base >= old_size) return 0;
  *ptr = new_base + (v - old_base);
  return 1;
}

int mods_relocate_slots(uint32_t *const *slots, int nslots, uint32_t old_base, uint32_t old_size,
                        uint32_t new_base) {
  int n = 0;
  for (int i = 0; i < nslots; i++)
    if (slots[i]) n += rebase(slots[i], old_base, old_size, new_base);
  return n;
}

int mods_relocate_fields(void *arr, size_t elem, size_t count, const size_t *offsets, int noff, uint32_t old_base,
                         uint32_t old_size, uint32_t new_base) {
  int n = 0;
  unsigned char *base = (unsigned char *)arr;
  for (size_t i = 0; i < count; i++)
    for (int k = 0; k < noff; k++) {
      if (offsets[k] + sizeof(uint32_t) > elem) continue;
      uint32_t v;
      memcpy(&v, base + i * elem + offsets[k], sizeof v);   // fields may be unaligned
      if (rebase(&v, old_base, old_size, new_base)) {
        memcpy(base + i * elem + offsets[k], &v, sizeof v);
        n++;
      }
    }
  return n;
}
