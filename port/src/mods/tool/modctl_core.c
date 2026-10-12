// modctl_core: the native helper behind tools/modctl (simgolf mod, task-11).
// It is built from the same sources as the game's mod platform, so the CLI
// and the game resolve the same files to the same order, conflicts and
// messages. Output is JSON for tools/modctl, or the game's own log lines.
//
//   modctl_core resolve --mods-dir D [--profile P] [--ini F] [--no-mods] [--text]
//   modctl_core manifest DIR
//   modctl_core loadorder FILE
//   modctl_core set-profile FILE NAME [ID...]     (creates or replaces the list)
//   modctl_core delete-profile FILE NAME
//   modctl_core set-active FILE NAME
//   modctl_core check-save SAVE.sve --mods-dir D [--profile P] [--ini F] [--no-mods]
//   modctl_core semver VERSION RANGE              (prints 1, 0 or -1)
//   modctl_core data dump|coverage|check|show ...  (task-19 data tables, see modctl_data.c)
//
// Exit code: 0 ok, 1 errors found (resolve, manifest), 2 bad usage or I/O.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods/mods_core.h"

static void jstr(const char *s) {
  if (!s) { fputs("null", stdout); return; }
  putchar('"');
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p == '"' || *p == '\\') printf("\\%c", *p);
    else if (*p == '\n') fputs("\\n", stdout);
    else if (*p < 0x20) printf("\\u%04x", *p);
    else putchar(*p);
  }
  putchar('"');
}

static void print_line(void *ctx, const char *line) {
  (void)ctx;
  printf("%s\n", line);
}

static int g_first;
static void json_line(void *ctx, const char *line) {
  (void)ctx;
  if (!g_first) putchar(',');
  g_first = 0;
  jstr(line);
}

typedef struct {
  const char *dir, *profile, *ini;
  int no_mods, text;
} Opts;

static int parse_opts(int argc, char **argv, int from, Opts *o) {
  memset(o, 0, sizeof *o);
  for (int i = from; i < argc; i++) {
    if (!strcmp(argv[i], "--mods-dir") && i + 1 < argc) o->dir = argv[++i];
    else if (!strcmp(argv[i], "--profile") && i + 1 < argc) o->profile = argv[++i];
    else if (!strcmp(argv[i], "--ini") && i + 1 < argc) o->ini = argv[++i];
    else if (!strcmp(argv[i], "--no-mods")) o->no_mods = 1;
    else if (!strcmp(argv[i], "--text")) o->text = 1;
    else {
      fprintf(stderr, "modctl_core: unknown option %s\n", argv[i]);
      return 0;
    }
  }
  if (!o->dir) {
    fprintf(stderr, "modctl_core: --mods-dir is required\n");
    return 0;
  }
  return 1;
}

// The game loads mods.ini into the store before resolving; so does this.
static void open_session(ModSession *ss, const Opts *o) {
  mods_store_clear();
  if (o->ini && !o->no_mods) mods_store_load_ini(MS_INI, o->ini);
  mods_session_open(ss, o->dir, o->profile, o->no_mods);
}

static int session_errors(const ModSession *ss) {
  return mdiags_count(&ss->res.diags, MD_ERROR) + mdiags_count(&ss->set.diags, MD_ERROR) + (ss->missing_profile ? 1 : 0) +
         (ss->lo.error ? 1 : 0);
}

static int cmd_resolve(const Opts *o) {
  ModSession ss;
  open_session(&ss, o);
  int errors = session_errors(&ss);
  if (o->text) {
    mods_session_report(&ss, o->dir, print_line, NULL);
  } else {
    char *json = mods_report_json(&ss.set, &ss.res, ss.profile);
    // Splice the session fields in front of the report's own.
    printf("{\"mods_dir\":");
    jstr(o->dir);
    printf(",\"load_order_exists\":%s,\"load_order_error\":", ss.lo.exists ? "true" : "false");
    jstr(ss.lo.error);
    printf(",\"missing_profile\":%s,\"switches\":%d,\"total_errors\":%d,", ss.missing_profile ? "true" : "false",
           ss.nswitches, errors);
    // The game's log lines too, for tools that compare with golf.log.
    printf("\"lines\":[");
    g_first = 1;
    mods_session_report(&ss, o->dir, json_line, NULL);
    printf("],%s\n", json ? json + 1 : "}");
    free(json);
  }
  mods_session_close(&ss);
  return errors ? 1 : 0;
}

static int cmd_manifest(const char *dir) {
  ModSet s = {0};
  ModManifest m;
  manifest_load(&m, dir, 0);
  if (!m.id) m.id = mods_strdup("?");
  int ok = m.error == NULL;
  modset_add(&s, &m);
  ModResolution r = {0};
  mods_compute_keys(&s, NULL, 0, &r);
  char *json = mods_report_json(&s, &r, NULL);
  printf("{\"ok\":%s,%s\n", ok ? "true" : "false", json ? json + 1 : "}");
  free(json);
  resolution_free(&r);
  modset_free(&s);
  return ok ? 0 : 1;
}

static int cmd_loadorder(const char *path) {
  ModLoadOrder lo;
  loadorder_read(&lo, path);
  printf("{\"exists\":%s,\"active\":", lo.exists ? "true" : "false");
  jstr(lo.active);
  printf(",\"error\":");
  jstr(lo.error);
  printf(",\"profiles\":{\"vanilla\":[]");
  for (int i = 0; i < lo.n; i++) {
    if (!strcmp(lo.p[i].name, "vanilla")) continue;
    putchar(',');
    jstr(lo.p[i].name);
    printf(":[");
    for (int j = 0; j < lo.p[i].nmods; j++) {
      if (j) putchar(',');
      jstr(lo.p[i].mods[j]);
    }
    putchar(']');
  }
  printf("}}\n");
  loadorder_free(&lo);
  return 0;
}

static int valid_profile(const char *name) {
  if (!mods_valid_id(name)) {
    fprintf(stderr, "modctl_core: profile names are lowercase letters, digits, '_', '-' or '.'\n");
    return 0;
  }
  if (!strcmp(name, "vanilla")) {
    fprintf(stderr, "modctl_core: the profile \"vanilla\" is the original game and always empty\n");
    return 0;
  }
  return 1;
}

static int load_for_edit(ModLoadOrder *lo, const char *path) {
  loadorder_read(lo, path);
  if (lo->error) {
    fprintf(stderr, "modctl_core: %s: %s (fix it by hand first)\n", path, lo->error);
    return 0;
  }
  return 1;
}

static int cmd_set_profile(const char *path, const char *name, char **ids, int n) {
  ModLoadOrder lo;
  if (!valid_profile(name) || !load_for_edit(&lo, path)) return 2;
  ModProfile *p = loadorder_profile(&lo, name, 1);
  for (int i = 0; i < p->nmods; i++) free(p->mods[i]);
  free(p->mods);
  p->mods = NULL;
  p->nmods = 0;
  for (int i = 0; i < n; i++) {
    int dup = 0;
    for (int j = 0; j < p->nmods; j++) dup |= !strcmp(p->mods[j], ids[i]);
    if (dup) continue;
    char **a = (char **)realloc(p->mods, (size_t)(p->nmods + 1) * sizeof *a);
    a[p->nmods++] = mods_strdup(ids[i]);
    p->mods = a;
  }
  int ok = loadorder_write(&lo, path);
  loadorder_free(&lo);
  return ok ? 0 : 2;
}

static int cmd_delete_profile(const char *path, const char *name) {
  ModLoadOrder lo;
  if (!valid_profile(name) || !load_for_edit(&lo, path)) return 2;
  for (int i = 0; i < lo.n; i++) {
    if (strcmp(lo.p[i].name, name)) continue;
    for (int j = 0; j < lo.p[i].nmods; j++) free(lo.p[i].mods[j]);
    free(lo.p[i].mods);
    free(lo.p[i].name);
    memmove(&lo.p[i], &lo.p[i + 1], (size_t)(lo.n - i - 1) * sizeof lo.p[0]);
    lo.n--;
    break;
  }
  if (lo.active && !strcmp(lo.active, name)) {
    free(lo.active);
    lo.active = mods_strdup("vanilla");
  }
  int ok = loadorder_write(&lo, path);
  loadorder_free(&lo);
  return ok ? 0 : 2;
}

static int cmd_set_active(const char *path, const char *name) {
  ModLoadOrder lo;
  if (!mods_valid_id(name) || !load_for_edit(&lo, path)) return 2;
  if (strcmp(name, "vanilla") && !loadorder_profile(&lo, name, 0)) {
    fprintf(stderr, "modctl_core: no profile \"%s\"\n", name);
    loadorder_free(&lo);
    return 2;
  }
  free(lo.active);
  lo.active = mods_strdup(name);
  int ok = loadorder_write(&lo, path);
  loadorder_free(&lo);
  return ok ? 0 : 2;
}

static int cmd_check_save(const char *save, const Opts *o) {
  uint32_t size, crc;
  if (!mods_file_crc(save, &size, &crc)) {
    fprintf(stderr, "modctl_core: cannot read %s\n", save);
    return 2;
  }
  char side[1024];
  snprintf(side, sizeof side, "%s.mods.toml", save);
  char *text = mods_read_file(side, NULL);
  ModSaveRec rec;
  int have = text && saverec_parse(&rec, text);
  free(text);
  int stale = have && (rec.save_size != size || rec.save_crc != crc);
  printf("{\"save\":");
  jstr(save);
  printf(",\"size\":%u,\"crc32\":\"%08x\",\"record\":%s,\"stale\":%s", (unsigned)size, (unsigned)crc,
         have ? "true" : "false", stale ? "true" : "false");
  int warns = 0;
  if (have && !stale) {
    printf(",\"format_version\":%d,\"platform\":", rec.format);
    jstr(rec.platform);
    printf(",\"profile\":");
    jstr(rec.profile);
    printf(",\"mods\":[");
    for (int i = 0; i < rec.n; i++) {
      printf(i ? ",{\"id\":" : "{\"id\":");
      jstr(rec.id[i]);
      printf(",\"version\":");
      jstr(rec.version[i]);
      putchar('}');
    }
    printf("],\"diagnostics\":[");
    ModSession ss;
    open_session(&ss, o);
    ModDiags ds = {0};
    warns = saverec_check(&rec, &ss.set, &ss.res, &ds);
    for (int i = 0; i < ds.n; i++) {
      printf(i ? ",{\"level\":" : "{\"level\":");
      jstr(mdiag_level_name(ds.d[i].level));
      printf(",\"code\":");
      jstr(ds.d[i].code);
      printf(",\"mod\":");
      jstr(ds.d[i].mod);
      printf(",\"message\":");
      jstr(ds.d[i].msg);
      putchar('}');
    }
    putchar(']');
    mdiags_free(&ds);
    mods_session_close(&ss);
  }
  printf(",\"warnings\":%d}\n", warns);
  return 0;
}

static int usage(void) {
  fprintf(stderr, "usage: modctl_core resolve|manifest|loadorder|set-profile|delete-profile|set-active|check-save|semver"
                  " ... (see the comment at the top of modctl_core.c)\n");
  return 2;
}

int modctl_data_main(int argc, char **argv);   // modctl_data.c

int main(int argc, char **argv) {
  if (argc < 2) return usage();
  const char *cmd = argv[1];
  if (!strcmp(cmd, "data")) return modctl_data_main(argc, argv);
  Opts o;
  if (!strcmp(cmd, "resolve")) return parse_opts(argc, argv, 2, &o) ? cmd_resolve(&o) : 2;
  if (!strcmp(cmd, "manifest") && argc == 3) return cmd_manifest(argv[2]);
  if (!strcmp(cmd, "loadorder") && argc == 3) return cmd_loadorder(argv[2]);
  if (!strcmp(cmd, "set-profile") && argc >= 4) return cmd_set_profile(argv[2], argv[3], argv + 4, argc - 4);
  if (!strcmp(cmd, "delete-profile") && argc == 4) return cmd_delete_profile(argv[2], argv[3]);
  if (!strcmp(cmd, "set-active") && argc == 4) return cmd_set_active(argv[2], argv[3]);
  if (!strcmp(cmd, "check-save") && argc >= 3) return parse_opts(argc, argv, 3, &o) ? cmd_check_save(argv[2], &o) : 2;
  if (!strcmp(cmd, "semver") && argc == 4) {
    printf("%d\n", semver_satisfies(argv[2], argv[3]));
    return 0;
  }
  return usage();
}
