// <strings.h> for the native Windows build (MSVC CRT), simgolf mod task-2.
// The port's C sources were written against Emscripten's musl.

#ifndef SIMGOLF_COMPAT_STRINGS_H
#define SIMGOLF_COMPAT_STRINGS_H

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

static inline char *strcasestr(const char *hay, const char *needle) {
  size_t n = strlen(needle);
  if (!n) return (char *)hay;
  for (; *hay; hay++)
    if (!_strnicmp(hay, needle, n)) return (char *)hay;
  return NULL;
}

#endif
