// <dirent.h> for the native Windows build, simgolf mod task-2 (dirent.c).

#ifndef SIMGOLF_COMPAT_DIRENT_H
#define SIMGOLF_COMPAT_DIRENT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DIR DIR;
struct dirent {
  char d_name[260];
};

DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);

#ifdef __cplusplus
}
#endif
#endif
