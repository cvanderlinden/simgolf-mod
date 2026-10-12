// The mod resolver, conflict detector and reports (simgolf mod, task-11).
// Deterministic: the same files give the same order and the same messages on
// every machine, in the game and in tools/modctl (both run this code).

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods/mods_core.h"

static int ieq(const char *a, const char *b) {
  for (; *a && *b; a++, b++)
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
  return *a == *b;
}

// ------------------------------------------------------------------ folding values

static int as_number(const char *s, double *d) {
  if (!s || !*s) return 0;
  char *end;
  *d = strtod(s, &end);
  return *end == 0;
}

static void number_text(double d, char *out, size_t outlen) {
  if (fabs(d) < 9e15 && d == floor(d)) snprintf(out, outlen, "%.0f", d);
  else snprintf(out, outlen, "%.10g", d);
}

char *mods_fold(const char *cur, int op, const char *val, char *out, size_t outlen) {
  double a, b;
  switch (op) {
    case MO_REMOVE:
      out[0] = 0;
      return out;
    case MO_ADD:
    case MO_MUL:
    case MO_MIN:
    case MO_MAX:
      if (!as_number(val, &b)) b = op == MO_MUL ? 1 : 0;
      // An unset value starts from the identity: 0 for add, 1 for mul.
      if (!as_number(cur, &a)) {
        if (op == MO_MIN || op == MO_MAX) { number_text(b, out, outlen); return out; }
        a = op == MO_MUL ? 1 : 0;
      }
      if (op == MO_ADD) a += b;
      else if (op == MO_MUL) a *= b;
      else if (op == MO_MIN) a = b < a ? b : a;
      else a = b > a ? b : a;
      number_text(a, out, outlen);
      return out;
    case MO_APPEND:
      if (cur && *cur) snprintf(out, outlen, "%s,%s", cur, val ? val : "");
      else snprintf(out, outlen, "%s", val ? val : "");
      return out;
    default:   // set, replace, merge of a scalar: the new value
      snprintf(out, outlen, "%s", val ? val : "");
      return out;
  }
}

// ------------------------------------------------------------------ keys and conflicts

static ModKeyUse *key_get(ModResolution *r, int kind, const char *key) {
  for (int i = 0; i < r->nkeys; i++)
    if (r->keys[i].kind == kind && ieq(r->keys[i].key, key)) return &r->keys[i];
  ModKeyUse *a = (ModKeyUse *)realloc(r->keys, (size_t)(r->nkeys + 1) * sizeof *a);
  if (!a) return NULL;
  r->keys = a;
  ModKeyUse *k = &r->keys[r->nkeys++];
  memset(k, 0, sizeof *k);
  k->kind = kind;
  k->key = mods_strdup(key);
  k->winner = -1;
  return k;
}

static void key_push(ModKeyUse *k, int mod, int op, const char *value) {
  int n = k->n + 1;
  k->mod = (int *)realloc(k->mod, (size_t)n * sizeof *k->mod);
  k->op = (int *)realloc(k->op, (size_t)n * sizeof *k->op);
  k->value = (char **)realloc(k->value, (size_t)n * sizeof *k->value);
  k->mod[k->n] = mod;
  k->op[k->n] = op;
  k->value[k->n] = mods_strdup(value);
  k->n = n;
}

static int distinct_mods(const ModKeyUse *k) {
  int d = 0;
  for (int i = 0; i < k->n; i++) {
    int seen = 0;
    for (int j = 0; j < i; j++) seen |= k->mod[j] == k->mod[i];
    d += !seen;
  }
  return d;
}

static void touch_text(const ModSet *s, const ModKeyUse *k, int i, char *out, size_t outlen) {
  const char *id = s->m[k->mod[i]].id;
  if (k->kind == MT_SETTING)
    snprintf(out, outlen, "%s %s %s", id, k->op[i] == MO_SET ? "sets" : mods_op_name(k->op[i]),
             k->value[i] ? k->value[i] : "");
  else
    snprintf(out, outlen, "%s %s", id, mods_op_name(k->op[i]));
}

void mods_compute_keys(const ModSet *s, const int *order, int norder, ModResolution *r) {
  for (int oi = 0; oi < norder; oi++) {
    const ModManifest *m = &s->m[order[oi]];
    for (int t = 0; t < m->ntouches; t++) {
      ModKeyUse *k = key_get(r, m->touches[t].kind, m->touches[t].key);
      if (k) key_push(k, order[oi], m->touches[t].op, m->touches[t].value);
    }
  }
  for (int i = 0; i < r->nkeys; i++) {
    ModKeyUse *k = &r->keys[i];
    char cur[512] = "", tmp[512];
    int has = 0, last_excl = -1;
    k->conflict = 0;
    for (int j = 0; j < k->n; j++) {
      if (mods_op_exclusive(k->op[j])) {
        for (int e = 0; e < j; e++)
          if (k->mod[e] != k->mod[j]) k->conflict = 1;
        last_excl = j;
      }
      if (k->kind == MT_SETTING) {
        mods_fold(has ? cur : NULL, k->op[j], k->value[j], tmp, sizeof tmp);
        snprintf(cur, sizeof cur, "%s", tmp);
        has = k->op[j] != MO_REMOVE;
      }
    }
    k->winner = last_excl >= 0 ? k->mod[last_excl] : -1;
    free(k->result);
    k->result = k->kind == MT_SETTING && has ? mods_strdup(cur) : NULL;
    if (distinct_mods(k) < 2) continue;
    // One line naming every touch in load order, then the outcome.
    char line[1500];
    size_t n = 0;
    for (int j = 0; j < k->n && n < sizeof line; j++) {
      char one[300];
      touch_text(s, k, j, one, sizeof one);
      n += (size_t)snprintf(line + n, sizeof line - n, "%s%s", j ? ", " : "", one);
    }
    if (k->conflict)
      mdiag_add(&r->diags, MD_WARN, "conflict", s->m[k->winner].id, "conflict on %s %s: %s; %s wins (it loads later)%s%s",
                mods_kind_name(k->kind), k->key, line, s->m[k->winner].id, k->result ? ", value " : "",
                k->result ? k->result : "");
    else
      mdiag_add(&r->diags, MD_NOTE, "merged", NULL, "merged %s %s: %s; all apply%s%s", mods_kind_name(k->kind), k->key,
                line, k->result ? ", value " : "", k->result ? k->result : "");
  }
}

// ------------------------------------------------------------------ resolver

typedef struct {
  const ModSet *s;
  int n;
  char *want;     // in the resolved set
  char *blocked;  // switched off in mods.ini: never enabled automatically
  int *rank;      // profile position (auto-enabled ones after)
  int nrank;
} Ctx;

// The enabled mod that answers to `id` (its own id or one it provides).
static int provider(const Ctx *c, const char *id, int except) {
  int i = modset_find(c->s, id);
  if (i >= 0 && i != except && c->want[i]) return i;
  for (int j = 0; j < c->n; j++) {
    if (!c->want[j] || j == except) continue;
    for (int p = 0; p < c->s->m[j].nprovides; p++)
      if (!strcmp(c->s->m[j].provides[p], id)) return j;
  }
  return -1;
}

// An installed mod that could be enabled to answer `id`.
static int candidate(const Ctx *c, const char *id) {
  int i = modset_find(c->s, id);
  if (i >= 0) return i;
  for (int j = 0; j < c->n; j++)
    for (int p = 0; p < c->s->m[j].nprovides; p++)
      if (!strcmp(c->s->m[j].provides[p], id)) return j;
  return -1;
}

static int usable(const Ctx *c, int i, ModResolution *r, int report) {
  const ModManifest *m = &c->s->m[i];
  if (m->error) {
    if (report)
      mdiag_add(&r->diags, MD_ERROR, "not_loaded", m->id, "%s is enabled but its mod.toml is broken; it is not loaded",
                m->id);
    return 0;
  }
  if (semver_satisfies(MODS_PLATFORM_VERSION, m->platform) != 1) {
    if (report)
      mdiag_add(&r->diags, MD_ERROR, "platform_version", m->id,
                "%s needs mod platform %s, this game has %s; it is not loaded", m->id, m->platform,
                MODS_PLATFORM_VERSION);
    return 0;
  }
  if (semver_satisfies(MODS_GAME_VERSION, m->game) != 1) {
    if (report)
      mdiag_add(&r->diags, MD_ERROR, "game_version", m->id, "%s needs game version %s, this is %s; it is not loaded",
                m->id, m->game, MODS_GAME_VERSION);
    return 0;
  }
  return 1;
}

// Sort key: built-ins first, then the profile order, then the id.
static int key_less(const Ctx *c, int a, int b) {
  const ModManifest *ma = &c->s->m[a], *mb = &c->s->m[b];
  if (ma->builtin != mb->builtin) return ma->builtin > mb->builtin;
  if (c->rank[a] != c->rank[b]) return c->rank[a] < c->rank[b];
  return strcmp(ma->id, mb->id) < 0;
}

void mods_resolve(const ModSet *s, const char *const *requested, int nreq, const ModSwitch *sw, int nsw,
                  ModResolution *r) {
  memset(r, 0, sizeof *r);
  Ctx c = {s, s->n, (char *)calloc((size_t)s->n + 1, 1), (char *)calloc((size_t)s->n + 1, 1),
           (int *)calloc((size_t)s->n + 1, sizeof(int)), 0};
  for (int i = 0; i < s->n; i++) c.rank[i] = 1 << 30;

  // 1. The requested mods: profile order, then mods.ini switches.
  for (int k = 0; k < nsw; k++) {
    if (sw[k].on) continue;
    int i = modset_find(s, sw[k].id);
    if (i >= 0) c.blocked[i] = 1;
  }
  int total = nreq + nsw;
  for (int k = 0; k < total; k++) {
    const char *id = k < nreq ? requested[k] : sw[k - nreq].on ? sw[k - nreq].id : NULL;
    if (!id) continue;
    int i = modset_find(s, id);
    if (k < nreq) {
      int off = 0;
      for (int q = 0; q < nsw; q++) off |= !sw[q].on && !strcmp(sw[q].id, id);
      if (off) {
        mdiag_add(&r->diags, MD_NOTE, "switched_off", id, "%s is in the profile but switched off in mods.ini", id);
        continue;
      }
    }
    if (i < 0) {
      mdiag_add(&r->diags, MD_ERROR, "missing_mod", id, "%s is enabled but not installed (no mods/%s/mod.toml)", id, id);
      continue;
    }
    if (c.want[i]) continue;
    if (!usable(&c, i, r, 1)) continue;
    c.want[i] = 1;
    c.rank[i] = c.nrank++;
    if (k >= nreq) mdiag_add(&r->diags, MD_NOTE, "switched_on", id, "%s is switched on in mods.ini", id);
  }

  // 2. Required dependencies that are installed but not enabled get enabled,
  //    in the order their dependents come (repeat for their own dependencies).
  for (int changed = 1; changed;) {
    changed = 0;
    for (int pass = 0; pass < c.nrank && !changed; pass++) {
      int i = -1;
      for (int j = 0; j < c.n; j++)
        if (c.want[j] && c.rank[j] == pass) i = j;
      if (i < 0) continue;
      const ModManifest *m = &s->m[i];
      for (int d = 0; d < m->ndeps; d++) {
        if (provider(&c, m->deps[d].id, i) >= 0) continue;
        int cand = candidate(&c, m->deps[d].id);
        if (cand < 0 || cand == i || c.blocked[cand] || !usable(&c, cand, r, 0)) continue;
        c.want[cand] = 1;
        c.rank[cand] = c.nrank++;
        mdiag_add(&r->diags, MD_NOTE, "auto_enabled", s->m[cand].id, "%s is enabled because %s needs it",
                  s->m[cand].id, m->id);
        changed = 1;
      }
    }
  }

  // 3. Dependencies and incompatibilities, until nothing else drops out (a
  //    mod that is not loaded takes the mods that need it with it).
  for (int changed = 1; changed;) {
    changed = 0;
    for (int pass = 0; pass < c.nrank; pass++) {
      int i = -1;
      for (int j = 0; j < c.n; j++)
        if (c.want[j] && c.rank[j] == pass) i = j;
      if (i < 0) continue;
      const ModManifest *m = &s->m[i];
      for (int d = 0; d < m->ndeps && c.want[i]; d++) {
        const ModDep *dep = &m->deps[d];
        int p = provider(&c, dep->id, i);
        if (p < 0) {
          int cand = candidate(&c, dep->id);
          if (cand < 0)
            mdiag_add(&r->diags, MD_ERROR, "missing_dependency", m->id,
                      "%s needs %s %s, which is not installed; %s is not loaded", m->id, dep->id, dep->range, m->id);
          else
            mdiag_add(&r->diags, MD_ERROR, "missing_dependency", m->id,
                      "%s needs %s %s, which is %s; %s is not loaded", m->id, dep->id, dep->range,
                      c.blocked[cand] ? "switched off in mods.ini" : "not loaded", m->id);
          c.want[i] = 0;
          changed = 1;
        } else if (semver_satisfies(s->m[p].version, dep->range) != 1) {
          mdiag_add(&r->diags, MD_ERROR, "dependency_version", m->id,
                    "%s needs %s %s, but %s is %s; %s is not loaded", m->id, dep->id, dep->range, s->m[p].id,
                    s->m[p].version, m->id);
          c.want[i] = 0;
          changed = 1;
        }
      }
      for (int d = 0; d < m->nincompat && c.want[i]; d++) {
        const ModDep *inc = &m->incompat[d];
        int p = provider(&c, inc->id, i);
        if (p < 0 || semver_satisfies(s->m[p].version, inc->range) != 1) continue;
        // The one that comes later in the profile gives way.
        int drop = c.rank[p] > c.rank[i] ? p : i, keep = drop == p ? i : p;
        mdiag_add(&r->diags, MD_ERROR, "incompatible", s->m[drop].id,
                  "%s and %s cannot be used together (%s says so); %s is not loaded because it comes later",
                  m->id, s->m[p].id, m->id, s->m[drop].id);
        (void)keep;
        c.want[drop] = 0;
        changed = 1;
      }
    }
  }

  // Optional dependencies: a note when absent, a warning on a wrong version.
  for (int i = 0; i < c.n; i++) {
    if (!c.want[i]) continue;
    const ModManifest *m = &s->m[i];
    for (int d = 0; d < m->nopt; d++) {
      int p = provider(&c, m->opt[d].id, i);
      if (p < 0)
        mdiag_add(&r->diags, MD_NOTE, "optional_missing", m->id, "%s can use %s, which is not loaded", m->id,
                  m->opt[d].id);
      else if (semver_satisfies(s->m[p].version, m->opt[d].range) != 1)
        mdiag_add(&r->diags, MD_WARN, "optional_version", m->id, "%s can use %s %s, but %s is %s: not used", m->id,
                  m->opt[d].id, m->opt[d].range, s->m[p].id, s->m[p].version);
    }
  }

  // 4. Order: edges u -> v mean u loads before v.
  int n = c.n;
  char *edge = (char *)calloc((size_t)n * (size_t)n + 1, 1);
  for (int v = 0; v < n; v++) {
    if (!c.want[v]) continue;
    const ModManifest *m = &s->m[v];
    for (int d = 0; d < m->ndeps; d++) {
      int u = provider(&c, m->deps[d].id, v);
      if (u >= 0) edge[u * n + v] = 1;
    }
    for (int d = 0; d < m->nopt; d++) {
      int u = provider(&c, m->opt[d].id, v);
      if (u >= 0 && semver_satisfies(s->m[u].version, m->opt[d].range) == 1) edge[u * n + v] = 1;
    }
    for (int d = 0; d < m->nafter; d++) {
      int u = provider(&c, m->after[d], v);
      if (u >= 0) edge[u * n + v] = 1;
    }
    for (int d = 0; d < m->nbefore; d++) {
      int u = provider(&c, m->before[d], v);
      if (u >= 0) edge[v * n + u] = 1;
    }
  }
  char *placed = (char *)calloc((size_t)n + 1, 1);
  r->order = (int *)malloc(((size_t)n + 1) * sizeof(int));
  int remaining = 0;
  for (int i = 0; i < n; i++) remaining += c.want[i];
  while (remaining) {
    int best = -1;
    for (int v = 0; v < n; v++) {
      if (!c.want[v] || placed[v]) continue;
      int ready = 1;
      for (int u = 0; u < n && ready; u++)
        if (edge[u * n + v] && c.want[u] && !placed[u]) ready = 0;
      if (ready && (best < 0 || key_less(&c, v, best))) best = v;
    }
    if (best < 0) {
      // A cycle: start at the first remaining mod by the tie-break, walk back
      // along unplaced predecessors (lowest key first) until a mod repeats.
      int start = -1;
      for (int v = 0; v < n; v++)
        if (c.want[v] && !placed[v] && (start < 0 || key_less(&c, v, start))) start = v;
      int *path = (int *)malloc(((size_t)n + 1) * sizeof(int)), len = 0, cur = start;
      int *pos = (int *)malloc(((size_t)n + 1) * sizeof(int));
      for (int v = 0; v < n; v++) pos[v] = -1;
      while (pos[cur] < 0) {
        pos[cur] = len;
        path[len++] = cur;
        int pred = -1;
        for (int u = 0; u < n; u++)
          if (edge[u * n + cur] && c.want[u] && !placed[u] && (pred < 0 || key_less(&c, u, pred))) pred = u;
        cur = pred;
      }
      // path[pos[cur]..len-1] walks backwards: reverse it into load order,
      // then print it starting at the mod that breaks the cycle.
      int clen = len - pos[cur], *cyc = (int *)malloc((size_t)clen * sizeof(int)), bi = 0;
      for (int j = 0; j < clen; j++) cyc[j] = path[len - 1 - j];
      for (int j = 1; j < clen; j++)
        if (key_less(&c, cyc[j], cyc[bi])) bi = j;
      int brk = cyc[bi];
      char line[1500];
      size_t k = 0;
      for (int j = 0; j <= clen && k < sizeof line; j++)
        k += (size_t)snprintf(line + k, sizeof line - k, "%s%s", j ? " -> " : "", s->m[cyc[(bi + j) % clen]].id);
      free(cyc);
      mdiag_add(&r->diags, MD_ERROR, "cycle", s->m[brk].id,
                "load order cycle: %s (each must load before the next); broken by loading %s first", line,
                s->m[brk].id);
      best = brk;
      free(path);
      free(pos);
    }
    placed[best] = 1;
    r->order[r->norder++] = best;
    remaining--;
  }
  free(placed);
  free(edge);
  free(c.want);
  free(c.blocked);
  free(c.rank);

  // 5. Who touches what: conflicts (the later mod wins) and merges.
  mods_compute_keys(s, r->order, r->norder, r);
}

int resolution_has(const ModResolution *r, int set_index) {
  for (int i = 0; i < r->norder; i++)
    if (r->order[i] == set_index) return 1;
  return 0;
}

void resolution_free(ModResolution *r) {
  free(r->order);
  mdiags_free(&r->diags);
  for (int i = 0; i < r->nkeys; i++) {
    ModKeyUse *k = &r->keys[i];
    for (int j = 0; j < k->n; j++) free(k->value[j]);
    free(k->key); free(k->mod); free(k->op); free(k->value); free(k->result);
  }
  free(r->keys);
  memset(r, 0, sizeof *r);
}

// ------------------------------------------------------------------ reports

static void emit(mods_print_fn fn, void *ctx, const char *fmt, ...) {
  char line[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  fn(ctx, line);
}

static void report_diags(const ModDiags *ds, mods_print_fn fn, void *ctx) {
  for (int i = 0; i < ds->n; i++)
    emit(fn, ctx, "%s %s: %s", mdiag_level_name(ds->d[i].level), ds->d[i].code, ds->d[i].msg);
}

void mods_report(const ModSet *s, const ModResolution *r, const char *profile, mods_print_fn fn, void *ctx) {
  int installed = s->n;
  emit(fn, ctx, "platform %s, game %s, %d mods installed, profile \"%s\", %d loaded", MODS_PLATFORM_VERSION,
       MODS_GAME_VERSION, installed, profile ? profile : "?", r->norder);
  for (int i = 0; i < r->norder; i++) {
    const ModManifest *m = &s->m[r->order[i]];
    emit(fn, ctx, "order %d: %s %s%s", i + 1, m->id, m->version, m->builtin ? " (built-in)" : "");
  }
  report_diags(&s->diags, fn, ctx);
  for (int i = 0; i < s->n; i++) report_diags(&s->m[i].warns, fn, ctx);
  report_diags(&r->diags, fn, ctx);
  int e = mdiags_count(&r->diags, MD_ERROR) + mdiags_count(&s->diags, MD_ERROR);
  int w = mdiags_count(&r->diags, MD_WARN) + mdiags_count(&s->diags, MD_WARN);
  for (int i = 0; i < s->n; i++) w += mdiags_count(&s->m[i].warns, MD_WARN);
  int conflicts = 0;
  for (int i = 0; i < r->nkeys; i++) conflicts += r->keys[i].conflict && distinct_mods(&r->keys[i]) > 1;
  emit(fn, ctx, "%d errors, %d warnings, %d conflicts", e, w, conflicts);
}

// ------------------------------------------------------------------ JSON

typedef struct {
  char *p;
  size_t n, cap;
} JBuf;

static void jput(JBuf *b, const char *s, size_t len) {
  if (b->n + len + 1 > b->cap) {
    size_t cap = b->cap ? b->cap : 4096;
    while (cap < b->n + len + 1) cap *= 2;
    char *p = (char *)realloc(b->p, cap);
    if (!p) return;
    b->p = p;
    b->cap = cap;
  }
  memcpy(b->p + b->n, s, len);
  b->n += len;
  b->p[b->n] = 0;
}

static void jraw(JBuf *b, const char *s) { jput(b, s, strlen(s)); }

static void jstr(JBuf *b, const char *s) {
  if (!s) { jraw(b, "null"); return; }
  jput(b, "\"", 1);
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    char esc[8];
    if (*p == '"' || *p == '\\') { esc[0] = '\\'; esc[1] = (char)*p; jput(b, esc, 2); }
    else if (*p == '\n') jput(b, "\\n", 2);
    else if (*p == '\r') jput(b, "\\r", 2);
    else if (*p == '\t') jput(b, "\\t", 2);
    else if (*p < 0x20) { snprintf(esc, sizeof esc, "\\u%04x", *p); jraw(b, esc); }
    else jput(b, (const char *)p, 1);
  }
  jput(b, "\"", 1);
}

static void jkv(JBuf *b, const char *k, const char *v, int comma) {
  if (comma) jraw(b, ",");
  jstr(b, k);
  jraw(b, ":");
  jstr(b, v);
}

static void jdiags(JBuf *b, const ModDiags *ds, int *first) {
  for (int i = 0; i < ds->n; i++) {
    jraw(b, *first ? "{" : ",{");
    *first = 0;
    jkv(b, "level", mdiag_level_name(ds->d[i].level), 0);
    jkv(b, "code", ds->d[i].code, 1);
    jkv(b, "mod", ds->d[i].mod, 1);
    jkv(b, "message", ds->d[i].msg, 1);
    jraw(b, "}");
  }
}

static void jstrs(JBuf *b, const char *key, char **a, int n) {
  jraw(b, ",");
  jstr(b, key);
  jraw(b, ":[");
  for (int i = 0; i < n; i++) {
    if (i) jraw(b, ",");
    jstr(b, a[i]);
  }
  jraw(b, "]");
}

static void jdeps(JBuf *b, const char *key, const ModDep *d, int n) {
  jraw(b, ",");
  jstr(b, key);
  jraw(b, ":{");
  for (int i = 0; i < n; i++) jkv(b, d[i].id, d[i].range, i > 0);
  jraw(b, "}");
}

char *mods_report_json(const ModSet *s, const ModResolution *r, const char *profile) {
  JBuf b = {0};
  char num[64];
  jraw(&b, "{");
  jkv(&b, "platform", MODS_PLATFORM_VERSION, 0);
  jkv(&b, "game", MODS_GAME_VERSION, 1);
  jkv(&b, "profile", profile, 1);
  jraw(&b, ",\"order\":[");
  for (int i = 0; i < r->norder; i++) {
    if (i) jraw(&b, ",");
    jstr(&b, s->m[r->order[i]].id);
  }
  jraw(&b, "],\"installed\":[");
  for (int i = 0; i < s->n; i++) {
    const ModManifest *m = &s->m[i];
    jraw(&b, i ? ",{" : "{");
    jkv(&b, "id", m->id, 0);
    jkv(&b, "name", m->name, 1);
    jkv(&b, "version", m->version, 1);
    jkv(&b, "description", m->description, 1);
    jkv(&b, "dir", m->dir, 1);
    jkv(&b, "platform", m->platform, 1);
    jkv(&b, "game", m->game, 1);
    jkv(&b, "error", m->error, 1);
    snprintf(num, sizeof num, ",\"builtin\":%s,\"loaded\":%s", m->builtin ? "true" : "false",
             resolution_has(r, i) ? "true" : "false");
    jraw(&b, num);
    jstrs(&b, "authors", m->authors, m->nauthors);
    jstrs(&b, "load_after", m->after, m->nafter);
    jstrs(&b, "load_before", m->before, m->nbefore);
    jstrs(&b, "provides", m->provides, m->nprovides);
    jdeps(&b, "dependencies", m->deps, m->ndeps);
    jdeps(&b, "optional_dependencies", m->opt, m->nopt);
    jdeps(&b, "incompatibilities", m->incompat, m->nincompat);
    jraw(&b, ",\"touches\":[");
    for (int t = 0; t < m->ntouches; t++) {
      jraw(&b, t ? ",{" : "{");
      jkv(&b, "kind", mods_kind_name(m->touches[t].kind), 0);
      jkv(&b, "key", m->touches[t].key, 1);
      jkv(&b, "op", mods_op_name(m->touches[t].op), 1);
      jkv(&b, "value", m->touches[t].value, 1);
      jraw(&b, "}");
    }
    jraw(&b, "]}");
  }
  jraw(&b, "],\"diagnostics\":[");
  int first = 1;
  jdiags(&b, &s->diags, &first);
  for (int i = 0; i < s->n; i++) jdiags(&b, &s->m[i].warns, &first);
  jdiags(&b, &r->diags, &first);
  jraw(&b, "],\"keys\":[");
  for (int i = 0; i < r->nkeys; i++) {
    const ModKeyUse *k = &r->keys[i];
    jraw(&b, i ? ",{" : "{");
    jkv(&b, "kind", mods_kind_name(k->kind), 0);
    jkv(&b, "key", k->key, 1);
    jkv(&b, "winner", k->winner >= 0 ? s->m[k->winner].id : NULL, 1);
    jkv(&b, "result", k->result, 1);
    snprintf(num, sizeof num, ",\"conflict\":%s,\"mods\":%d,\"touches\":[", k->conflict ? "true" : "false",
             distinct_mods(k));
    jraw(&b, num);
    for (int j = 0; j < k->n; j++) {
      jraw(&b, j ? ",{" : "{");
      jkv(&b, "mod", s->m[k->mod[j]].id, 0);
      jkv(&b, "op", mods_op_name(k->op[j]), 1);
      jkv(&b, "value", k->value[j], 1);
      jraw(&b, "}");
    }
    jraw(&b, "]}");
  }
  int e = mdiags_count(&r->diags, MD_ERROR) + mdiags_count(&s->diags, MD_ERROR);
  snprintf(num, sizeof num, "],\"errors\":%d}", e);
  jraw(&b, num);
  return b.p;
}
