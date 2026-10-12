// <unistd.h> for the native Windows build (MSVC CRT), simgolf mod task-2.
// Only what the port's sources use. Files open in binary mode: host_win32.c
// sets _fmode to _O_BINARY at start, as musl has no text mode.

#ifndef SIMGOLF_COMPAT_UNISTD_H
#define SIMGOLF_COMPAT_UNISTD_H

#include <direct.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

typedef intptr_t ssize_t;

// POSIX mkdir(path, mode); Windows has no mode bits.
#define mkdir(path, mode) _mkdir(path)
#define ftruncate(fd, size) _chsize((fd), (long)(size))

static inline ssize_t pread(int fd, void *buf, size_t n, long off) {
  long cur = _lseek(fd, 0, SEEK_CUR);
  if (_lseek(fd, off, SEEK_SET) < 0) return -1;
  int r = _read(fd, buf, (unsigned)n);
  _lseek(fd, cur, SEEK_SET);
  return r;
}

#endif
