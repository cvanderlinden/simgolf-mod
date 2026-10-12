// opendir/readdir over FindFirstFileA (see dirent.h).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dirent.h"

struct DIR {
  HANDLE h;
  WIN32_FIND_DATAA fd;
  int first;
  struct dirent ent;
};

DIR *opendir(const char *path) {
  char pat[MAX_PATH + 4];
  snprintf(pat, sizeof pat, "%s/*", path);
  DIR *d = calloc(1, sizeof *d);
  if (!d) return NULL;
  d->h = FindFirstFileA(pat, &d->fd);
  if (d->h == INVALID_HANDLE_VALUE) {
    free(d);
    return NULL;
  }
  d->first = 1;
  return d;
}

struct dirent *readdir(DIR *d) {
  if (!d->first && !FindNextFileA(d->h, &d->fd)) return NULL;
  d->first = 0;
  snprintf(d->ent.d_name, sizeof d->ent.d_name, "%s", d->fd.cFileName);
  return &d->ent;
}

int closedir(DIR *d) {
  if (!d) return -1;
  FindClose(d->h);
  free(d);
  return 0;
}
