// The startup decision shared by the game and tools/modctl (simgolf mod,
// task-11): profile, mods.ini switches, scan, resolve, report.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods/mods_core.h"

void mods_session_open(ModSession *ss, const char *mods_dir, const char *profile, int no_mods) {
  memset(ss, 0, sizeof *ss);
  char p[800];
  if (mods_dir && *mods_dir) {
    snprintf(p, sizeof p, "%s/load_order.toml", mods_dir);
    loadorder_read(&ss->lo, p);
  } else {
    loadorder_read(&ss->lo, "");
  }
  const char *prof = no_mods ? "vanilla" : profile && *profile ? profile : ss->lo.active;
  snprintf(ss->profile, sizeof ss->profile, "%s", prof ? prof : "vanilla");

  const char *keys[256], *vals[256];
  int nsw = no_mods ? 0 : mods_store_layer_keys(MS_INI, "mods.", keys, vals, 256);
  ModSwitch sw[256];
  for (int i = 0; i < nsw; i++) {
    sw[i].id = keys[i] + 5;
    sw[i].on = atoi(vals[i]) != 0;
  }
  ss->nswitches = nsw;
  if (mods_dir && *mods_dir) modset_scan(&ss->set, mods_dir);
  ModProfile *pr = loadorder_profile(&ss->lo, ss->profile, 0);
  ss->missing_profile = !pr && strcmp(ss->profile, "vanilla");
  mods_resolve(&ss->set, pr ? (const char *const *)pr->mods : NULL, pr ? pr->nmods : 0, sw, nsw, &ss->res);
}

static void say(mods_print_fn fn, void *ctx, const char *fmt, const char *a) {
  char line[1200];
  snprintf(line, sizeof line, fmt, a);
  fn(ctx, line);
}

void mods_session_report(const ModSession *ss, const char *mods_dir, mods_print_fn fn, void *ctx) {
  say(fn, ctx, "folder %s", mods_dir && *mods_dir ? mods_dir : "(none)");
  say(fn, ctx, "load order %s", ss->lo.exists ? "load_order.toml" : "(no load_order.toml: vanilla)");
  if (ss->lo.error) say(fn, ctx, "error load_order: %s (ignored)", ss->lo.error);
  if (ss->missing_profile)
    say(fn, ctx, "error missing_profile: profile \"%s\" is not in load_order.toml; no mods loaded", ss->profile);
  mods_report(&ss->set, &ss->res, ss->profile, fn, ctx);
}

void mods_session_close(ModSession *ss) {
  modset_free(&ss->set);
  resolution_free(&ss->res);
  loadorder_free(&ss->lo);
  memset(ss, 0, sizeof *ss);
}
