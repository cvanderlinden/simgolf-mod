// modctl_core data ...: the mod data layer offline (simgolf mod, task-19).
// The same code as the game (data.c), reading golf.exe's image from
// golf.bin (build/native/image/golf.bin) instead of the running game.
//
//   modctl_core data dump --tables DIR --image golf.bin --out DIR
//       base rows of every table as DIR/<table>.json, plus coverage.json
//   modctl_core data coverage --tables DIR
//       every table and field with where it lives (JSON)
//   modctl_core data check --tables DIR [--image golf.bin] --mods-dir D [--profile P] [--ini F] [--no-mods] [--text]
//       applies the loaded mods' data files like the game: problems with
//       file:line, conflicts and merges, every value that changes
//   modctl_core data show TABLE (same options as check)
//       the table after the mods (JSON)
//
// Exit code: 0 ok, 1 problems found, 2 bad usage or I/O.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods/data.h"
#include "mods/mods_core.h"

typedef struct {
  const char *tables, *image, *out, *dir, *profile, *ini, *table;
  int no_mods, text;
} DOpts;

static int dparse(int argc, char **argv, int from, DOpts *o) {
  memset(o, 0, sizeof *o);
  for (int i = from; i < argc; i++) {
    const char *a = argv[i];
    if (!strcmp(a, "--tables") && i + 1 < argc) o->tables = argv[++i];
    else if (!strcmp(a, "--image") && i + 1 < argc) o->image = argv[++i];
    else if (!strcmp(a, "--out") && i + 1 < argc) o->out = argv[++i];
    else if (!strcmp(a, "--mods-dir") && i + 1 < argc) o->dir = argv[++i];
    else if (!strcmp(a, "--profile") && i + 1 < argc) o->profile = argv[++i];
    else if (!strcmp(a, "--ini") && i + 1 < argc) o->ini = argv[++i];
    else if (!strcmp(a, "--no-mods")) o->no_mods = 1;
    else if (!strcmp(a, "--text")) o->text = 1;
    else if (a[0] != '-' && !o->table) o->table = a;
    else {
      fprintf(stderr, "modctl_core data: unknown option %s\n", a);
      return 0;
    }
  }
  if (!o->tables) {
    fprintf(stderr, "modctl_core data: --tables is required\n");
    return 0;
  }
  return 1;
}

static DataImageFile g_imgf;
static DataImage g_img;

static int load(DataSet *ds, const DOpts *o) {
  memset(ds, 0, sizeof *ds);
  data_load_defs(ds, o->tables);
  if (o->image) {
    if (!data_image_file_open(&g_imgf, o->image, 0x400000u)) {
      fprintf(stderr, "modctl_core data: cannot read %s\n", o->image);
      return 0;
    }
    g_img = data_image_of_file(&g_imgf);
  }
  data_build_base(ds, o->image ? &g_img : NULL);
  return 1;
}

static int write_file(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");
  if (!f) return 0;
  fputs(text, f);
  fclose(f);
  return 1;
}

static int cmd_dump(const DOpts *o) {
  DataSet ds;
  if (!o->out || !o->image) {
    fprintf(stderr, "modctl_core data dump: --image and --out are required\n");
    return 2;
  }
  if (!load(&ds, o)) return 2;
  int bad = 0;
  for (int i = 0; i < ds.n; i++) {
    char path[900];
    snprintf(path, sizeof path, "%s/%s.json", o->out, ds.t[i].name);
    JsonVal *v = data_table_json(&ds.t[i], 0);
    char *s = json_write(v);
    if (!write_file(path, s)) { fprintf(stderr, "cannot write %s\n", path); bad = 1; }
    free(s);
    json_free(v);
  }
  char path[900];
  snprintf(path, sizeof path, "%s/coverage.json", o->out);
  JsonVal *cov = data_coverage_json(&ds);
  char *s = json_write(cov);
  write_file(path, s);
  free(s);
  json_free(cov);
  for (int d = 0; d < ds.diags.n; d++) fprintf(stderr, "%s %s: %s\n", mdiag_level_name(ds.diags.d[d].level), ds.diags.d[d].code, ds.diags.d[d].msg);
  printf("%d tables written to %s\n", ds.n, o->out);
  int errors = mdiags_count(&ds.diags, MD_ERROR);
  data_free(&ds);
  data_image_file_close(&g_imgf);
  return bad ? 2 : errors ? 1 : 0;
}

static int cmd_coverage(const DOpts *o) {
  DataSet ds;
  if (!load(&ds, o)) return 2;
  JsonVal *cov = data_coverage_json(&ds);
  char *s = json_write(cov);
  fputs(s, stdout);
  free(s);
  json_free(cov);
  int errors = mdiags_count(&ds.diags, MD_ERROR);
  data_free(&ds);
  data_image_file_close(&g_imgf);
  return errors ? 1 : 0;
}

// ------------------------------------------------------------------ check, show

static ModSession g_ss;

static int touch_cb(void *ctx, const char *mod, const char *key, const char *op) {
  (void)ctx;
  for (int j = 0; j < g_ss.res.norder; j++) {
    ModManifest *m = &g_ss.set.m[g_ss.res.order[j]];
    if (strcmp(m->id, mod)) continue;
    int o = mods_op_parse(op);
    for (int t = 0; t < m->ntouches; t++)   // declared in mod.toml already: counts once (as in the game)
      if (m->touches[t].kind == MT_DATA && m->touches[t].op == o && !_stricmp(m->touches[t].key, key)) return 1;
    ModTouch *a = (ModTouch *)realloc(m->touches, (size_t)(m->ntouches + 1) * sizeof *a);
    if (!a) return -1;
    m->touches = a;
    memset(&a[m->ntouches], 0, sizeof *a);
    a[m->ntouches].kind = MT_DATA;
    a[m->ntouches].op = mods_op_parse(op);
    a[m->ntouches].key = mods_strdup(key);
    m->ntouches++;
    return 1;
  }
  return -1;
}

static void print_lines(const DataSet *ds, char **lines, int n, const DOpts *o, int tables_rows) {
  if (o->text) {
    for (int i = 0; i < n; i++) printf("data: %s\n", lines[i]);
    return;
  }
  printf("{\"tables\":%d,\"rows\":%d,\"changes\":%d,\"errors\":%d,\"lines\":[", ds->n, tables_rows, ds->changes,
         mdiags_count(&ds->diags, MD_ERROR));
  for (int i = 0; i < n; i++) {
    JsonVal *s = json_str(lines[i]);
    char *t = json_write_compact(s);
    printf("%s%s", i ? "," : "", t);
    free(t);
    json_free(s);
  }
  printf("]}\n");
}

static char **g_lines;
static int g_n;
static void add_line(void *ctx, const char *line) {
  (void)ctx;
  g_lines = (char **)realloc(g_lines, (size_t)(g_n + 1) * sizeof *g_lines);
  g_lines[g_n++] = mods_strdup(line);
}

static int cmd_check(const DOpts *o, int show) {
  if (!o->dir) {
    fprintf(stderr, "modctl_core data: --mods-dir is required\n");
    return 2;
  }
  DataSet ds;
  if (!load(&ds, o)) return 2;
  mods_store_clear();
  if (o->ini && !o->no_mods) mods_store_load_ini(MS_INI, o->ini);
  mods_session_open(&g_ss, o->dir, o->profile, o->no_mods);
  int rows = 0;
  for (int i = 0; i < ds.n; i++) rows += ds.t[i].nfixed;
  char line[1200];
  snprintf(line, sizeof line, "%d tables, %d game rows from %s", ds.n, rows, o->tables);
  add_line(NULL, line);
  int before_keys = g_ss.res.diags.n;
  for (int i = 0; i < g_ss.res.norder; i++) {
    const ModManifest *m = &g_ss.set.m[g_ss.res.order[i]];
    int n = data_apply_mod(&ds, m->id, m->dir, touch_cb, NULL);
    if (n) {
      snprintf(line, sizeof line, "mod %s: %d changes", m->id, n);
      add_line(NULL, line);
    }
  }
  for (int d = 0; d < ds.diags.n; d++) {
    const ModDiag *dg = &ds.diags.d[d];
    snprintf(line, sizeof line, "%s %s%s%s%s: %s", mdiag_level_name(dg->level), dg->code, dg->mod ? " (" : "",
             dg->mod ? dg->mod : "", dg->mod ? ")" : "", dg->msg);
    add_line(NULL, line);
  }
  // Conflicts and merges of the data keys, the way the game's registry sees them.
  (void)before_keys;
  for (int q = 0; q < g_ss.res.nkeys; q++) {
    ModKeyUse *u = &g_ss.res.keys[q];
    for (int j = 0; j < u->n; j++) free(u->value[j]);
    free(u->key); free(u->mod); free(u->op); free(u->value); free(u->result);
  }
  free(g_ss.res.keys);
  g_ss.res.keys = NULL;
  g_ss.res.nkeys = 0;
  int keep = 0;
  for (int d = 0; d < g_ss.res.diags.n; d++) {
    ModDiag *dg = &g_ss.res.diags.d[d];
    if (!strcmp(dg->code, "conflict") || !strcmp(dg->code, "merged")) { free(dg->mod); free(dg->msg); continue; }
    g_ss.res.diags.d[keep++] = *dg;
  }
  g_ss.res.diags.n = keep;
  int b = g_ss.res.diags.n;
  mods_compute_keys(&g_ss.set, g_ss.res.order, g_ss.res.norder, &g_ss.res);
  for (int d = b; d < g_ss.res.diags.n; d++) {
    const ModDiag *dg = &g_ss.res.diags.d[d];
    if (!strstr(dg->msg, " data ")) continue;
    snprintf(line, sizeof line, "%s %s: %s", mdiag_level_name(dg->level), dg->code, dg->msg);
    add_line(NULL, line);
  }
  int w = 0;
  if (o->image) {
    // What the game would write (on a copy of the image).
    w = data_write_image(&ds, &g_img, NULL, NULL);
  }
  data_report_changes(&ds, add_line, NULL);
  snprintf(line, sizeof line, "%d values changed by mods, %d written to the game's tables, %d problems", ds.changes, w,
           mdiags_count(&ds.diags, MD_ERROR));
  add_line(NULL, line);
  int errors = mdiags_count(&ds.diags, MD_ERROR);
  if (show) {
    DataTable *t = data_table(&ds, o->table);
    if (!t) {
      fprintf(stderr, "modctl_core data show: no table %s\n", o->table ? o->table : "(none)");
      errors = 1;
    } else {
      JsonVal *v = data_table_json(t, 1);
      char *s = json_write(v);
      fputs(s, stdout);
      free(s);
      json_free(v);
    }
    for (int i = 0; i < g_n; i++) fprintf(stderr, "data: %s\n", g_lines[i]);
  } else {
    print_lines(&ds, g_lines, g_n, o, rows);
  }
  mods_session_close(&g_ss);
  data_free(&ds);
  data_image_file_close(&g_imgf);
  return errors ? 1 : 0;
}

int modctl_data_main(int argc, char **argv) {
  if (argc < 3) return 2;
  const char *sub = argv[2];
  DOpts o;
  if (!dparse(argc, argv, 3, &o)) return 2;
  if (!strcmp(sub, "dump")) return cmd_dump(&o);
  if (!strcmp(sub, "coverage")) return cmd_coverage(&o);
  if (!strcmp(sub, "check")) return cmd_check(&o, 0);
  if (!strcmp(sub, "show")) return cmd_check(&o, 1);
  fprintf(stderr, "modctl_core data: dump | coverage | check | show\n");
  return 2;
}
