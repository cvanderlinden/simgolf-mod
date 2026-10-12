// Mod data layer core (simgolf mod, task-19). See data.h and docs/modding/DATA.md.

#include "mods/data.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------------ helpers

static void dmsg(DataSet *ds, int level, const char *code, const char *mod, const char *fmt, ...) {
  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  mdiag_add(&ds->diags, level, code, mod, "%s", msg);
}

static int cmp_str(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int valid_row_id(const char *s) {
  if (!s || !*s || strlen(s) > 63) return 0;
  for (; *s; s++)
    if (!(islower((unsigned char)*s) || isdigit((unsigned char)*s) || *s == '_' || *s == '-')) return 0;
  return 1;
}

static int utf8_len(const char *s) {
  int n = 0;
  for (; *s; s++)
    if (((unsigned char)*s & 0xc0) != 0x80) n++;
  return n;
}

// Latin-1 (the game's text) to UTF-8 and back.
static char *latin1_to_utf8(const unsigned char *s, size_t n) {
  char *out = (char *)malloc(n * 2 + 1), *o = out;
  for (size_t i = 0; i < n && s[i]; i++) {
    unsigned c = s[i];
    if (c < 0x80) *o++ = (char)c;
    else { *o++ = (char)(0xc0 | c >> 6); *o++ = (char)(0x80 | (c & 0x3f)); }
  }
  *o = 0;
  return out;
}

// Returns the Latin-1 length, or -1 when a character has no Latin-1 form.
static int utf8_to_latin1(const char *s, char *out, size_t outlen) {
  size_t n = 0;
  const unsigned char *p = (const unsigned char *)s;
  while (*p) {
    unsigned cp;
    if (*p < 0x80) cp = *p++;
    else if ((*p & 0xe0) == 0xc0 && (p[1] & 0xc0) == 0x80) { cp = (unsigned)(*p & 0x1f) << 6 | (p[1] & 0x3f); p += 2; }
    else return -1;
    if (cp > 0xff) return -1;
    if (n + 1 < outlen) out[n] = (char)cp;
    n++;
  }
  if (outlen) out[n < outlen ? n : outlen - 1] = 0;
  return (int)n;
}

static int image_type(const char *s) {
  static const char *n[] = {"u8", "i8", "u16", "i16", "u32", "i32", "str", "strptr"};
  for (int i = 0; s && i < 8; i++)
    if (!strcmp(s, n[i])) return i;
  return -1;
}

static const char *image_type_name(int t) {
  static const char *n[] = {"u8", "i8", "u16", "i16", "u32", "i32", "str", "strptr"};
  return t >= 0 && t < 8 ? n[t] : "?";
}

static int num_of(const JsonVal *v, double *out) {
  if (!v) return 0;
  if (v->type == JS_NUM) { *out = v->n; return 1; }
  if (v->type == JS_STR) {   // "0x4c26b0"
    char *end;
    double d = (double)strtoul(v->s, &end, 0);
    if (end != v->s && !*end) { *out = d; return 1; }
  }
  return 0;
}

// ------------------------------------------------------------------ JSON Schema subset

static int check_at(const JsonVal *s, const JsonVal *v, const DataSet *ds, const char *path, char *msg,
                    size_t msglen);

static void say(char *msg, size_t msglen, const char *path, const char *fmt, ...) {
  char tmp[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof tmp, fmt, ap);
  va_end(ap);
  if (path && *path) snprintf(msg, msglen, "%s: %s", path, tmp);
  else snprintf(msg, msglen, "%s", tmp);
}

static void short_val(const JsonVal *v, char *out, size_t n) {
  char *s = json_write_compact(v);
  if (strlen(s) > 40) strcpy(s + 37, "...");
  snprintf(out, n, "%s", s);
  free(s);
}

static int type_ok(const char *t, const JsonVal *v) {
  if (!strcmp(t, "integer")) return json_is_int(v);
  if (!strcmp(t, "number")) return v->type == JS_NUM;
  if (!strcmp(t, "string")) return v->type == JS_STR;
  if (!strcmp(t, "boolean")) return v->type == JS_BOOL;
  if (!strcmp(t, "array")) return v->type == JS_ARR;
  if (!strcmp(t, "object")) return v->type == JS_OBJ;
  if (!strcmp(t, "null")) return v->type == JS_NULL;
  return 0;
}

static int check_at(const JsonVal *s, const JsonVal *v, const DataSet *ds, const char *path, char *msg,
                    size_t msglen) {
  if (!s || s->type != JS_OBJ) return 1;
  char sv[64];
  const JsonVal *k;
  if ((k = json_get(s, "type"))) {
    int ok = 0;
    char want[80] = "";
    if (k->type == JS_STR) { ok = type_ok(k->s, v); snprintf(want, sizeof want, "%s", k->s); }
    else if (k->type == JS_ARR)
      for (int i = 0; i < k->len; i++)
        if (k->items[i]->type == JS_STR) {
          ok |= type_ok(k->items[i]->s, v);
          size_t l = strlen(want);
          snprintf(want + l, sizeof want - l, "%s%s", i ? " or " : "", k->items[i]->s);
        }
    if (!ok) {
      short_val(v, sv, sizeof sv);
      say(msg, msglen, path, "expected %s %s, got %s %s", strchr("aeiou", want[0]) ? "an" : "a", want,
          json_type_name(v->type), sv);
      return 0;
    }
  }
  if ((k = json_get(s, "enum")) && k->type == JS_ARR) {
    int ok = 0;
    for (int i = 0; i < k->len && !ok; i++) ok = json_equal(k->items[i], v);
    if (!ok) {
      char opts[300] = "";
      for (int i = 0; i < k->len && strlen(opts) < 240; i++) {
        char one[64];
        short_val(k->items[i], one, sizeof one);
        size_t l = strlen(opts);
        snprintf(opts + l, sizeof opts - l, "%s%s", i ? ", " : "", one);
      }
      short_val(v, sv, sizeof sv);
      say(msg, msglen, path, "%s is not one of %s", sv, opts);
      return 0;
    }
  }
  if ((k = json_get(s, "const")) && !json_equal(k, v)) {
    short_val(k, sv, sizeof sv);
    say(msg, msglen, path, "must be %s", sv);
    return 0;
  }
  if (v->type == JS_NUM) {
    double lim;
    if (num_of(json_get(s, "minimum"), &lim) && v->n < lim) {
      say(msg, msglen, path, "%g is below the minimum %g", v->n, lim);
      return 0;
    }
    if (num_of(json_get(s, "maximum"), &lim) && v->n > lim) {
      say(msg, msglen, path, "%g is above the maximum %g", v->n, lim);
      return 0;
    }
    if (num_of(json_get(s, "exclusiveMinimum"), &lim) && v->n <= lim) {
      say(msg, msglen, path, "%g must be above %g", v->n, lim);
      return 0;
    }
    if (num_of(json_get(s, "exclusiveMaximum"), &lim) && v->n >= lim) {
      say(msg, msglen, path, "%g must be below %g", v->n, lim);
      return 0;
    }
  }
  if (v->type == JS_STR) {
    double lim;
    int n = utf8_len(v->s);
    if (num_of(json_get(s, "maxLength"), &lim) && n > lim) {
      say(msg, msglen, path, "\"%.40s\" is %d characters, at most %g fit", v->s, n, lim);
      return 0;
    }
    if (num_of(json_get(s, "minLength"), &lim) && n < lim) {
      say(msg, msglen, path, "at least %g characters", lim);
      return 0;
    }
  }
  if (v->type == JS_ARR) {
    double lim;
    if (num_of(json_get(s, "maxItems"), &lim) && v->len > lim) {
      say(msg, msglen, path, "%d items, at most %g", v->len, lim);
      return 0;
    }
    if (num_of(json_get(s, "minItems"), &lim) && v->len < lim) {
      say(msg, msglen, path, "%d items, at least %g", v->len, lim);
      return 0;
    }
    if ((k = json_get(s, "items")))
      for (int i = 0; i < v->len; i++) {
        char p[200];
        snprintf(p, sizeof p, "%s[%d]", path, i);
        if (!check_at(k, v->items[i], ds, p, msg, msglen)) return 0;
      }
  }
  if (v->type == JS_OBJ) {
    const JsonVal *props = json_get(s, "properties"), *addl = json_get(s, "additionalProperties");
    const JsonVal *names = json_get(s, "propertyNames"), *from = json_get(s, "x-keys-from");
    if ((k = json_get(s, "required")) && k->type == JS_ARR)
      for (int i = 0; i < k->len; i++)
        if (k->items[i]->type == JS_STR && !json_get(v, k->items[i]->s)) {
          say(msg, msglen, path, "\"%s\" is required", k->items[i]->s);
          return 0;
        }
    for (int i = 0; i < v->len; i++) {
      char p[200];
      snprintf(p, sizeof p, "%s%s%s", path, *path ? "." : "", v->keys[i]);
      const JsonVal *ps = json_get(props, v->keys[i]);
      if (ps) {
        if (!check_at(ps, v->items[i], ds, p, msg, msglen)) return 0;
        continue;
      }
      if (names) {
        JsonVal *key = json_str(v->keys[i]);
        int ok = check_at(names, key, ds, p, msg, msglen);
        json_free(key);
        if (!ok) return 0;
      }
      if (from && from->type == JS_STR && ds) {
        const DataTable *t = data_table(ds, from->s);
        if (t && data_row_index(t, v->keys[i]) < 0) {
          say(msg, msglen, path, "\"%s\" is not a row of %s", v->keys[i], from->s);
          return 0;
        }
      }
      if (addl && addl->type == JS_BOOL && !addl->b) {
        say(msg, msglen, path, "unknown field \"%s\"", v->keys[i]);
        return 0;
      }
      if (addl && addl->type == JS_OBJ && !check_at(addl, v->items[i], ds, p, msg, msglen)) return 0;
    }
  }
  return 1;
}

int data_schema_check(const JsonVal *schema, const JsonVal *v, const DataSet *ds, char *msg, size_t msglen) {
  if (msglen) msg[0] = 0;
  return check_at(schema, v, ds, "", msg, msglen);
}

// ------------------------------------------------------------------ tables

DataTable *data_table(const DataSet *ds, const char *name) {
  for (int i = 0; ds && name && i < ds->n; i++)
    if (!strcmp(ds->t[i].name, name)) return &ds->t[i];
  return NULL;
}

int data_row_index(const DataTable *t, const char *id) {
  for (int i = 0; t && id && i < t->nrows; i++)
    if (t->row[i] && !strcmp(t->row_id[i], id)) return i;
  return -1;
}

static int any_row_index(const DataTable *t, const char *id) {   // removed rows too
  for (int i = 0; i < t->nrows; i++)
    if (!strcmp(t->row_id[i], id)) return i;
  return -1;
}

int data_field_index(const DataTable *t, const char *name) {
  for (int i = 0; t && name && i < t->nf; i++)
    if (!strcmp(t->f[i].name, name)) return i;
  return -1;
}

const JsonVal *data_value(const DataTable *t, int row, int field) {
  if (!t || row < 0 || row >= t->nrows || field < 0 || field >= t->nf || !t->row[row]) return NULL;
  return json_get(t->row[row], t->f[field].name);
}

static int add_row(DataTable *t, const char *id) {
  if (t->nrows == t->cap) {
    t->cap = t->cap ? t->cap * 2 : 16;
    t->row_id = (char **)realloc(t->row_id, (size_t)t->cap * sizeof *t->row_id);
    t->base = (JsonVal **)realloc(t->base, (size_t)t->cap * sizeof *t->base);
    t->row = (JsonVal **)realloc(t->row, (size_t)t->cap * sizeof *t->row);
    t->row_mod = (char **)realloc(t->row_mod, (size_t)t->cap * sizeof *t->row_mod);
    t->row_touched = (unsigned char *)realloc(t->row_touched, (size_t)t->cap);
    t->touched = (unsigned char *)realloc(t->touched, (size_t)t->cap * (size_t)(t->nf ? t->nf : 1));
  }
  int r = t->nrows++;
  t->row_id[r] = mods_strdup(id);
  t->base[r] = NULL;
  t->row[r] = json_new(JS_OBJ);
  t->row_mod[r] = NULL;
  t->row_touched[r] = 0;
  memset(t->touched + (size_t)r * (size_t)(t->nf ? t->nf : 1), 0, (size_t)(t->nf ? t->nf : 1));
  return r;
}

static int parse_def(DataSet *ds, DataTable *t) {
  const char *p = t->path;
  JsonVal *d = t->def;
  if (d->type != JS_OBJ) { dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: a table definition is a JSON object", p, d->line); return 0; }
  const JsonVal *xt = json_get(d, "x-table"), *props = json_get(d, "properties"), *k;
  if (!xt || xt->type != JS_OBJ) { dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: \"x-table\" is missing", p, d->line); return 0; }
  if (!props || props->type != JS_OBJ) { dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: \"properties\" is missing", p, d->line); return 0; }
  if ((k = json_get(d, "title")) && k->type == JS_STR) t->title = mods_strdup(k->s);
  t->extensible = (k = json_get(xt, "extensible")) && k->type == JS_BOOL && k->b;
  t->single = (k = json_get(xt, "single")) && k->type == JS_BOOL && k->b;
  const JsonVal *img = json_get(xt, "image");
  if (img) {
    double b, s;
    if (img->type != JS_OBJ || !num_of(json_get(img, "base"), &b) || !num_of(json_get(img, "stride"), &s) || s <= 0) {
      dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: \"image\" needs \"base\" and \"stride\"", p, img->line);
      return 0;
    }
    t->has_image = 1;
    t->image_base = (uint32_t)b;
    t->image_stride = (uint32_t)s;
  }
  t->f = (DataField *)calloc((size_t)props->len + 1, sizeof *t->f);
  for (int i = 0; i < props->len; i++) {
    DataField *f = &t->f[t->nf];
    JsonVal *fs = props->items[i];
    f->name = mods_strdup(props->keys[i]);
    f->schema = fs;
    f->readonly = (k = json_get(fs, "readOnly")) && k->type == JS_BOOL && k->b;
    f->integer = (k = json_get(fs, "type")) && k->type == JS_STR && !strcmp(k->s, "integer");
    const JsonVal *xi = json_get(fs, "x-image");
    if (xi) {
      double off, len = 0;
      const JsonVal *ty = json_get(xi, "type");
      f->itype = ty && ty->type == JS_STR ? image_type(ty->s) : -1;
      if (!t->has_image || xi->type != JS_OBJ || !num_of(json_get(xi, "offset"), &off) || f->itype < 0) {
        dmsg(ds, MD_ERROR, "data_def", NULL,
             "%s:%d: %s: \"x-image\" needs the table's image, an \"offset\" and a \"type\" (u8 i8 u16 i16 u32 i32 str strptr)",
             p, xi->line, f->name);
        return 0;
      }
      num_of(json_get(xi, "len"), &len);
      if (f->itype == DI_STR && len < 2) {
        dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: %s: a \"str\" field needs \"len\"", p, xi->line, f->name);
        return 0;
      }
      f->bind = DB_IMAGE;
      f->offset = (uint32_t)off;
      f->len = (uint32_t)len;
    } else if ((k = json_get(fs, "x-code")) && k->type == JS_BOOL && k->b) {
      f->bind = DB_CODE;
    }
    t->nf++;
  }
  const JsonVal *rows = json_get(xt, "rows");
  if (rows && rows->type != JS_ARR) { dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: \"rows\" is a list", p, rows->line); return 0; }
  for (int i = 0; rows && i < rows->len; i++) {
    const JsonVal *r = rows->items[i], *id = json_get(r, "id");
    if (r->type != JS_OBJ || !id || id->type != JS_STR || !valid_row_id(id->s) || any_row_index(t, id->s) >= 0) {
      dmsg(ds, MD_ERROR, "data_def", NULL, "%s:%d: rows[%d] needs a unique \"id\" (a-z, 0-9, _ and -)", p, r->line, i);
      return 0;
    }
    add_row(t, id->s);
  }
  t->nfixed = t->nrows;
  return 1;
}

int data_load_defs(DataSet *ds, const char *tables_dir) {
  DIR *d = opendir(tables_dir);
  if (!d) {
    dmsg(ds, MD_ERROR, "data_def", NULL, "%s: the tables folder cannot be read", tables_dir);
    return 0;
  }
  char **names = NULL;
  int n = 0;
  struct dirent *e;
  while ((e = readdir(d))) {
    size_t l = strlen(e->d_name);
    if (l <= 5 || strcmp(e->d_name + l - 5, ".json") || e->d_name[0] == '.' || e->d_name[0] == '_') continue;
    names = (char **)realloc(names, (size_t)(n + 1) * sizeof *names);
    names[n++] = mods_strdup(e->d_name);
  }
  closedir(d);
  qsort(names, (size_t)n, sizeof *names, cmp_str);
  for (int i = 0; i < n; i++) {
    char path[800], err[600];
    snprintf(path, sizeof path, "%s/%s", tables_dir, names[i]);
    JsonVal *def = json_load(path, err, sizeof err);
    if (!def) {
      dmsg(ds, MD_ERROR, "data_def", NULL, "%s", err);
      free(names[i]);
      continue;
    }
    ds->t = (DataTable *)realloc(ds->t, (size_t)(ds->n + 1) * sizeof *ds->t);
    DataTable *t = &ds->t[ds->n];
    memset(t, 0, sizeof *t);
    names[i][strlen(names[i]) - 5] = 0;
    t->name = names[i];
    t->path = mods_strdup(path);
    t->def = def;
    ds->n++;
    if (!parse_def(ds, t)) {
      // keep the name so mods get a clear error, but no rows
      for (int r = 0; r < t->nrows; r++) { free(t->row_id[r]); json_free(t->row[r]); }
      t->nrows = t->nfixed = 0;
      t->nf = 0;
    }
  }
  free(names);
  return ds->n;
}

static JsonVal *read_image_field(const DataTable *t, const DataField *f, int r, const DataImage *img) {
  uint32_t a = t->image_base + (uint32_t)r * t->image_stride + f->offset;
  int sz[] = {1, 1, 2, 2, 4, 4, 0, 4};
  if (f->itype == DI_STR) {
    unsigned char *s = img->at(img->ctx, a, f->len);
    if (!s) return NULL;
    char *u = latin1_to_utf8(s, f->len);
    JsonVal *v = json_str(u);
    free(u);
    return v;
  }
  unsigned char *b = img->at(img->ctx, a, (uint32_t)sz[f->itype]);
  if (!b) return NULL;
  double n = 0;
  switch (f->itype) {
    case DI_U8: n = b[0]; break;
    case DI_I8: n = (signed char)b[0]; break;
    case DI_U16: n = (uint16_t)(b[0] | b[1] << 8); break;
    case DI_I16: n = (int16_t)(b[0] | b[1] << 8); break;
    case DI_U32: n = (uint32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24); break;
    case DI_I32: n = (int32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24); break;
    case DI_STRPTR: {
      uint32_t ptr = (uint32_t)(b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24);
      unsigned char *s = ptr ? img->at(img->ctx, ptr, 1) : NULL;
      if (!s) return json_str("");
      size_t len = 0;
      while (len < 255 && img->at(img->ctx, ptr + (uint32_t)len, 1) && s[len]) len++;
      char *u = latin1_to_utf8(s, len);
      JsonVal *v = json_str(u);
      free(u);
      return v;
    }
  }
  return json_num(n);
}

void data_build_base(DataSet *ds, const DataImage *img) {
  for (int ti = 0; ti < ds->n; ti++) {
    DataTable *t = &ds->t[ti];
    const JsonVal *rows = json_get(json_get(t->def, "x-table"), "rows");
    for (int r = 0; r < t->nfixed; r++) {
      JsonVal *row = json_new(JS_OBJ);
      const JsonVal *dr = rows ? rows->items[r] : NULL;
      for (int fi = 0; fi < t->nf; fi++) {
        DataField *f = &t->f[fi];
        JsonVal *v = NULL;
        if (f->bind == DB_IMAGE) {
          if (img) v = read_image_field(t, f, r, img);
        } else {
          const JsonVal *dv = json_get(dr, f->name);
          if (dv) v = json_clone(dv);
        }
        if (v) json_set(row, f->name, v);
      }
      json_free(t->row[r]);
      json_free(t->base[r]);
      t->row[r] = row;
      t->base[r] = json_clone(row);
    }
  }
}

// ------------------------------------------------------------------ applying mods

typedef struct {
  DataSet *ds;
  DataTable *t;
  const char *mod, *file;
  data_touch_fn touch;
  void *ctx;
  int applied;
} Ctx;

static void touch(Ctx *c, const char *key, const char *op) {
  if (c->touch) c->touch(c->ctx, c->mod, key, op);
}

static void bad(Ctx *c, int line, const char *fmt, ...) {
  char msg[700];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  dmsg(c->ds, MD_ERROR, "data_error", c->mod, "%s:%d: %s", c->file, line, msg);
}

static double round_half_away(double x) { return x < 0 ? -floor(-x + 0.5) : floor(x + 0.5); }

// Splits "row.field" (or "field" in a single-row table).
static int split_path(Ctx *c, const char *path, int line, char *row, size_t rl, char *field, size_t fl) {
  const char *dot = strchr(path, '.');
  if (!dot) {
    if (c->t->single && c->t->nrows > 0) {
      snprintf(row, rl, "%s", c->t->row_id[0]);
      snprintf(field, fl, "%s", path);
      return 1;
    }
    bad(c, line, "\"%s\": write \"row.field\" (for example \"%s.%s\")", path, c->t->nrows ? c->t->row_id[0] : "row",
        c->t->nf ? c->t->f[0].name : "field");
    return 0;
  }
  snprintf(row, rl, "%.*s", (int)(dot - path), path);
  snprintf(field, fl, "%s", dot + 1);
  return 1;
}

static int check_field(Ctx *c, int line, int r, int fi, const JsonVal *v) {
  char msg[600];
  if (!data_schema_check(c->t->f[fi].schema, v, c->ds, msg, sizeof msg)) {
    bad(c, line, "%s.%s.%s: %s", c->t->name, c->t->row_id[r], c->t->f[fi].name, msg);
    return 0;
  }
  return 1;
}

static void store(Ctx *c, int r, int fi, JsonVal *nv) {
  DataTable *t = c->t;
  const JsonVal *old = json_get(t->row[r], t->f[fi].name);
  if (!json_equal(old, nv)) c->ds->changes++;
  json_set(t->row[r], t->f[fi].name, nv);
  t->touched[(size_t)r * (size_t)t->nf + (size_t)fi] = 1;
  t->row_touched[r] = 1;
  c->applied++;
}

static int op_parse_field(const char *s) {
  static const char *n[] = {"set", "add", "mul", "min", "max", "append"};
  for (int i = 0; i < 6; i++)
    if (!strcmp(s, n[i])) return i;
  return -1;
}

// One field change on one row.
static void field_op(Ctx *c, int r, int fi, int op, const JsonVal *val) {
  static const char *names[] = {"set", "add", "mul", "min", "max", "append"};
  DataTable *t = c->t;
  DataField *f = &t->f[fi];
  int line = val->line;
  if (f->readonly) { bad(c, line, "%s.%s.%s cannot be changed (read only)", t->name, t->row_id[r], f->name); return; }
  const JsonVal *cur = json_get(t->row[r], f->name);
  if (!cur && r < t->nfixed && f->bind != DB_NONE) {
    bad(c, line, "%s.%s has no %s in this game", t->name, t->row_id[r], f->name);
    return;
  }
  JsonVal *nv = NULL;
  if (op == 0) nv = json_clone(val);
  else if (op >= 1 && op <= 4) {
    if (val->type != JS_NUM) { bad(c, line, "\"%s\" needs a number", names[op]); return; }
    if (cur && cur->type != JS_NUM) {
      bad(c, line, "%s.%s.%s is not a number", t->name, t->row_id[r], f->name);
      return;
    }
    // No value yet (a field of a new row without a default): add starts
    // at 0, mul at 1, min and max take the given number.
    double y = val->n, x = cur ? cur->n : op == 1 ? 0 : op == 2 ? 1 : y;
    double z = op == 1 ? x + y : op == 2 ? x * y : op == 3 ? (x < y ? x : y) : (x > y ? x : y);
    if (f->integer) z = round_half_away(z);
    nv = json_num(z);
  } else {   // append
    if (cur && cur->type != JS_ARR) { bad(c, line, "%s.%s.%s is not a list", t->name, t->row_id[r], f->name); return; }
    nv = cur ? json_clone(cur) : json_new(JS_ARR);
    if (val->type == JS_ARR)
      for (int i = 0; i < val->len; i++) json_push(nv, json_clone(val->items[i]));
    else json_push(nv, json_clone(val));
  }
  nv->line = line;
  if (!check_field(c, line, r, fi, nv)) { json_free(nv); return; }
  char key[300];
  snprintf(key, sizeof key, "%s.%s.%s", t->name, t->row_id[r], f->name);
  touch(c, key, names[op]);
  store(c, r, fi, nv);
}

static void do_field_ops(Ctx *c, int op, const JsonVal *obj) {
  DataTable *t = c->t;
  if (obj->type != JS_OBJ) { bad(c, obj->line, "this operation takes an object of \"row.field\": value"); return; }
  for (int i = 0; i < obj->len; i++) {
    char row[128], field[128];
    const JsonVal *val = obj->items[i];
    if (!split_path(c, obj->keys[i], val->line, row, sizeof row, field, sizeof field)) continue;
    int fi = data_field_index(t, field);
    if (fi < 0) { bad(c, val->line, "%s has no field \"%s\"", t->name, field); continue; }
    if (!strcmp(row, "*")) {
      int n = 0;
      for (int r = 0; r < t->nrows; r++)
        if (t->row[r] && (op == 0 || json_get(t->row[r], field))) { field_op(c, r, fi, op, val); n++; }
      if (!n) bad(c, val->line, "no row of %s has %s", t->name, field);
      continue;
    }
    int r = data_row_index(t, row);
    if (r < 0) { bad(c, val->line, "%s has no row \"%s\"", t->name, row); continue; }
    field_op(c, r, fi, op, val);
  }
}

static JsonVal *deep_merge(const JsonVal *cur, const JsonVal *add) {
  if (!cur || cur->type != JS_OBJ || add->type != JS_OBJ) return json_clone(add);
  JsonVal *out = json_clone(cur);
  for (int i = 0; i < add->len; i++) json_set(out, add->keys[i], deep_merge(json_get(out, add->keys[i]), add->items[i]));
  out->line = add->line;
  return out;
}

static int check_row(Ctx *c, int line, int r) {
  char msg[600];
  if (!data_schema_check(c->t->def, c->t->row[r], c->ds, msg, sizeof msg)) {
    bad(c, line, "%s.%s: %s", c->t->name, c->t->row_id[r], msg);
    return 0;
  }
  return 1;
}

static JsonVal *defaults_row(const DataTable *t) {
  JsonVal *row = json_new(JS_OBJ);
  for (int fi = 0; fi < t->nf; fi++) {
    const JsonVal *d = json_get(t->f[fi].schema, "default");
    if (d) json_set(row, t->f[fi].name, json_clone(d));
  }
  return row;
}

static void row_merge(Ctx *c, const char *id, const JsonVal *rec, int replace) {
  DataTable *t = c->t;
  int line = rec->line;
  if (rec->type != JS_OBJ) { bad(c, line, "\"%s\" needs an object of fields", id); return; }
  if (!valid_row_id(id)) { bad(c, line, "\"%s\" is not a row id (a-z, 0-9, _ and -, at most 63)", id); return; }
  for (int i = 0; i < rec->len; i++)
    if (data_field_index(t, rec->keys[i]) < 0) {
      bad(c, rec->items[i]->line, "%s has no field \"%s\"", t->name, rec->keys[i]);
      return;
    }
  char key[300];
  snprintf(key, sizeof key, "%s.%s", t->name, id);
  int r = data_row_index(t, id);
  if (r < 0) {
    if (!t->extensible) { bad(c, line, "%s has no row \"%s\" (rows cannot be added to this table)", t->name, id); return; }
    int old = any_row_index(t, id);
    r = old >= 0 ? old : add_row(t, id);
    json_free(t->row[r]);
    t->row[r] = defaults_row(t);
    for (int i = 0; i < rec->len; i++) json_set(t->row[r], rec->keys[i], json_clone(rec->items[i]));
    if (!check_row(c, line, r)) {
      json_free(t->row[r]);
      t->row[r] = NULL;
      return;
    }
    free(t->row_mod[r]);
    t->row_mod[r] = mods_strdup(c->mod);
    for (int fi = 0; fi < t->nf; fi++) t->touched[(size_t)r * (size_t)t->nf + (size_t)fi] = 1;
    t->row_touched[r] = 1;
    touch(c, key, "add");
    c->ds->changes++;
    c->applied++;
    return;
  }
  if (replace) {
    JsonVal *row = r < t->nfixed && t->base[r] ? json_new(JS_OBJ) : defaults_row(t);
    for (int fi = 0; fi < t->nf; fi++) {
      DataField *f = &t->f[fi];
      const JsonVal *nv = json_get(rec, f->name), *cur = json_get(t->row[r], f->name);
      if (f->readonly) {
        if (nv && !json_equal(nv, cur)) {
          bad(c, nv->line, "%s.%s.%s cannot be changed (read only)", t->name, id, f->name);
          json_free(row);
          return;
        }
        if (cur) json_set(row, f->name, json_clone(cur));
      } else if (nv) {
        json_set(row, f->name, json_clone(nv));
      } else if (r < t->nfixed && cur && f->bind != DB_NONE) {
        bad(c, line, "%s.%s: \"replace\" of a game row needs every field (%s is missing)", t->name, id, f->name);
        json_free(row);
        return;
      }
    }
    JsonVal *keep = t->row[r];
    t->row[r] = row;
    if (!check_row(c, line, r)) {
      json_free(row);
      t->row[r] = keep;
      return;
    }
    for (int fi = 0; fi < t->nf; fi++) {
      const JsonVal *a = json_get(keep, t->f[fi].name), *b = json_get(row, t->f[fi].name);
      if (!json_equal(a, b)) { t->touched[(size_t)r * (size_t)t->nf + (size_t)fi] = 1; c->ds->changes++; }
    }
    json_free(keep);
    t->row_touched[r] = 1;
    touch(c, key, "set");
    c->applied++;
    return;
  }
  // merge into an existing row: field by field
  touch(c, key, "merge");
  for (int i = 0; i < rec->len; i++) {
    int fi = data_field_index(t, rec->keys[i]);
    const JsonVal *nv = rec->items[i], *cur = json_get(t->row[r], rec->keys[i]);
    DataField *f = &t->f[fi];
    if (f->readonly) { bad(c, nv->line, "%s.%s.%s cannot be changed (read only)", t->name, id, f->name); continue; }
    if (!cur && r < t->nfixed && f->bind != DB_NONE) { bad(c, nv->line, "%s.%s has no %s in this game", t->name, id, f->name); continue; }
    JsonVal *m = deep_merge(cur, nv);
    m->line = nv->line;
    if (!check_field(c, nv->line, r, fi, m)) { json_free(m); continue; }
    char fk[300];
    snprintf(fk, sizeof fk, "%s.%s.%s", t->name, id, f->name);
    if (cur && cur->type == JS_OBJ && nv->type == JS_OBJ) {
      touch(c, fk, "merge");
      for (int j = 0; j < nv->len; j++) {
        char kk[400];
        snprintf(kk, sizeof kk, "%s.%s", fk, nv->keys[j]);
        touch(c, kk, "set");
      }
    } else {
      touch(c, fk, "set");
    }
    store(c, r, fi, m);
  }
}

static void do_row_ops(Ctx *c, const JsonVal *obj, int replace) {
  if (obj->type != JS_OBJ) { bad(c, obj->line, "\"%s\" takes an object of row: {fields}", replace ? "replace" : "merge"); return; }
  for (int i = 0; i < obj->len; i++) row_merge(c, obj->keys[i], obj->items[i], replace);
}

static void do_remove(Ctx *c, const JsonVal *list) {
  DataTable *t = c->t;
  if (list->type != JS_ARR) { bad(c, list->line, "\"remove\" takes a list of row ids"); return; }
  for (int i = 0; i < list->len; i++) {
    const JsonVal *v = list->items[i];
    if (v->type != JS_STR) { bad(c, v->line, "\"remove\" takes row ids (strings)"); continue; }
    int r = data_row_index(t, v->s);
    if (r < 0) { bad(c, v->line, "%s has no row \"%s\"", t->name, v->s); continue; }
    if (!t->extensible || r < t->nfixed) {
      bad(c, v->line, "%s.%s is part of the game and cannot be removed (set its values instead)", t->name, v->s);
      continue;
    }
    char key[300];
    snprintf(key, sizeof key, "%s.%s", t->name, v->s);
    touch(c, key, "remove");
    json_free(t->row[r]);
    t->row[r] = NULL;
    t->row_touched[r] = 1;
    c->ds->changes++;
    c->applied++;
  }
}

int data_apply_file(DataSet *ds, const char *mod_id, const char *table, const char *path, data_touch_fn tf,
                    void *ctx) {
  Ctx c = {ds, data_table(ds, table), mod_id, path, tf, ctx, 0};
  char err[700];
  if (!c.t) {
    dmsg(ds, MD_ERROR, "data_error", mod_id, "%s:1: there is no table \"%s\" (see docs/modding/DATA.md)", path, table);
    return 0;
  }
  JsonVal *doc = json_load(path, err, sizeof err);
  if (!doc) { dmsg(ds, MD_ERROR, "data_error", mod_id, "%s", err); return 0; }
  if (doc->type != JS_OBJ) {
    bad(&c, doc->line, "a data file is a JSON object of operations (set, add, mul, min, max, append, merge, replace, remove)");
    json_free(doc);
    return 0;
  }
  // Operations run in a fixed order whatever the file order: rows first
  // (so field changes can address new rows), then field changes.
  static const char *order[] = {"remove", "replace", "merge", "set", "add", "mul", "min", "max", "append"};
  for (int i = 0; i < doc->len; i++) {
    const char *k = doc->keys[i];
    int known = !strcmp(k, "$schema") || !strcmp(k, "description") || !strcmp(k, "comment") || !strcmp(k, "title");
    for (int j = 0; j < 9 && !known; j++) known = !strcmp(k, order[j]);
    if (!known)
      bad(&c, doc->items[i]->line, "unknown operation \"%s\" (set, add, mul, min, max, append, merge, replace, remove)", k);
  }
  for (int j = 0; j < 9; j++) {
    const JsonVal *v = json_get(doc, order[j]);
    if (!v) continue;
    if (j == 0) do_remove(&c, v);
    else if (j <= 2) do_row_ops(&c, v, j == 1);
    else do_field_ops(&c, op_parse_field(order[j]), v);
  }
  json_free(doc);
  return c.applied;
}

int data_apply_mod(DataSet *ds, const char *mod_id, const char *mod_dir, data_touch_fn tf, void *ctx) {
  char dir[800];
  snprintf(dir, sizeof dir, "%s/data", mod_dir);
  DIR *d = opendir(dir);
  if (!d) return 0;
  char **names = NULL;
  int n = 0, applied = 0;
  struct dirent *e;
  while ((e = readdir(d))) {
    size_t l = strlen(e->d_name);
    if (l <= 5 || strcmp(e->d_name + l - 5, ".json") || e->d_name[0] == '.') continue;
    names = (char **)realloc(names, (size_t)(n + 1) * sizeof *names);
    names[n++] = mods_strdup(e->d_name);
  }
  closedir(d);
  qsort(names, (size_t)n, sizeof *names, cmp_str);
  for (int i = 0; i < n; i++) {
    char path[1000];
    snprintf(path, sizeof path, "%s/%s", dir, names[i]);
    names[i][strlen(names[i]) - 5] = 0;
    applied += data_apply_file(ds, mod_id, names[i], path, tf, ctx);
    free(names[i]);
  }
  free(names);
  return applied;
}

// ------------------------------------------------------------------ writing back

int data_write_image(DataSet *ds, const DataImage *img, data_log_fn log, void *ctx) {
  int written = 0;
  int sz[] = {1, 1, 2, 2, 4, 4, 0, 4};
  for (int ti = 0; ti < ds->n; ti++) {
    DataTable *t = &ds->t[ti];
    if (!t->has_image) continue;
    for (int r = 0; r < t->nfixed; r++) {
      if (!t->row_touched[r] || !t->row[r]) continue;
      for (int fi = 0; fi < t->nf; fi++) {
        DataField *f = &t->f[fi];
        if (f->bind != DB_IMAGE || !t->touched[(size_t)r * (size_t)t->nf + (size_t)fi]) continue;
        const JsonVal *v = json_get(t->row[r], f->name), *b = t->base[r] ? json_get(t->base[r], f->name) : NULL;
        if (!v || json_equal(v, b)) continue;
        uint32_t a = t->image_base + (uint32_t)r * t->image_stride + f->offset;
        char line[600], shown[80], was[80];
        short_val(v, shown, sizeof shown);
        if (b) short_val(b, was, sizeof was); else snprintf(was, sizeof was, "none");
        if (f->itype == DI_STR || f->itype == DI_STRPTR) {
          char l1[300];
          int n = v->type == JS_STR ? utf8_to_latin1(v->s, l1, sizeof l1) : -1;
          if (n < 0 || (f->itype == DI_STR && (uint32_t)n >= f->len)) {
            dmsg(ds, MD_ERROR, "data_error", NULL, "%s.%s.%s: %s does not fit the game's text (Latin-1, %u bytes)",
                 t->name, t->row_id[r], f->name, shown, f->itype == DI_STR ? f->len - 1 : 255u);
            continue;
          }
          if (f->itype == DI_STR) {
            unsigned char *p = img->at(img->ctx, a, f->len);
            if (!p) continue;
            memset(p, 0, f->len);
            memcpy(p, l1, (size_t)n);
          } else {
            unsigned char *p = img->at(img->ctx, a, 4);
            char *s = mods_strdup(l1);   // lives as long as the game
            uint32_t ptr = (uint32_t)(uintptr_t)s;
            if (!p || !s) continue;
            memcpy(p, &ptr, 4);
          }
        } else {
          unsigned char *p = img->at(img->ctx, a, (uint32_t)sz[f->itype]);
          if (!p || v->type != JS_NUM) continue;
          int64_t x = (int64_t)v->n;
          for (int i = 0; i < sz[f->itype]; i++) p[i] = (unsigned char)(x >> (8 * i));
        }
        written++;
        if (log) {
          snprintf(line, sizeof line, "%s.%s.%s = %s (was %s) at 0x%06x", t->name, t->row_id[r], f->name, shown, was, a);
          log(ctx, line);
        }
      }
    }
  }
  return written;
}

int data_report_changes(const DataSet *ds, data_log_fn log, void *ctx) {
  int n = 0;
  char line[700], shown[80], was[80];
  for (int ti = 0; ti < ds->n; ti++) {
    const DataTable *t = &ds->t[ti];
    for (int r = 0; r < t->nrows; r++) {
      if (!t->row_touched[r]) continue;
      if (!t->row[r]) {
        snprintf(line, sizeof line, "%s.%s removed", t->name, t->row_id[r]);
        if (log) log(ctx, line);
        n++;
        continue;
      }
      if (r >= t->nfixed) {
        snprintf(line, sizeof line, "%s.%s added by %s", t->name, t->row_id[r], t->row_mod[r] ? t->row_mod[r] : "?");
        if (log) log(ctx, line);
        n++;
        continue;
      }
      for (int fi = 0; fi < t->nf; fi++) {
        const DataField *f = &t->f[fi];
        if (!t->touched[(size_t)r * (size_t)t->nf + (size_t)fi]) continue;
        const JsonVal *v = json_get(t->row[r], f->name), *b = t->base[r] ? json_get(t->base[r], f->name) : NULL;
        if (!v || json_equal(v, b)) continue;
        short_val(v, shown, sizeof shown);
        if (b) short_val(b, was, sizeof was); else snprintf(was, sizeof was, "none");
        if (f->bind == DB_IMAGE)
          snprintf(line, sizeof line, "%s.%s.%s = %s (was %s; game table at 0x%06x)", t->name, t->row_id[r], f->name,
                   shown, was, t->image_base + (uint32_t)r * t->image_stride + f->offset);
        else
          snprintf(line, sizeof line, "%s.%s.%s = %s (was %s)", t->name, t->row_id[r], f->name, shown, was);
        if (log) log(ctx, line);
        n++;
      }
    }
  }
  return n;
}

// ------------------------------------------------------------------ export

JsonVal *data_table_json(const DataTable *t, int effective) {
  JsonVal *o = json_new(JS_OBJ), *rows = json_new(JS_OBJ);
  json_set(o, "table", json_str(t->name));
  if (t->title) json_set(o, "title", json_str(t->title));
  json_set(o, "source", json_str(effective ? "after the loaded mods" : "the game (base data)"));
  for (int r = 0; r < t->nrows; r++) {
    const JsonVal *v = effective ? t->row[r] : t->base[r];
    if (v) json_set(rows, t->row_id[r], json_clone(v));
  }
  json_set(o, "rows", rows);
  return o;
}

JsonVal *data_coverage_json(const DataSet *ds) {
  JsonVal *list = json_new(JS_ARR);
  for (int ti = 0; ti < ds->n; ti++) {
    const DataTable *t = &ds->t[ti];
    JsonVal *o = json_new(JS_OBJ), *fields = json_new(JS_ARR);
    json_set(o, "table", json_str(t->name));
    if (t->title) json_set(o, "title", json_str(t->title));
    json_set(o, "rows", json_num(t->nfixed));
    json_set(o, "extensible", json_bool(t->extensible));
    if (t->has_image) {
      char a[32];
      snprintf(a, sizeof a, "0x%06x", t->image_base);
      json_set(o, "image_base", json_str(a));
      json_set(o, "image_stride", json_num(t->image_stride));
    }
    for (int fi = 0; fi < t->nf; fi++) {
      const DataField *f = &t->f[fi];
      JsonVal *fo = json_new(JS_OBJ);
      json_set(fo, "field", json_str(f->name));
      json_set(fo, "bind", json_str(f->bind == DB_IMAGE ? "image" : f->bind == DB_CODE ? "code" : "data"));
      if (f->bind == DB_IMAGE) {
        char a[64];
        snprintf(a, sizeof a, "+0x%x %s", f->offset, image_type_name(f->itype));
        json_set(fo, "where", json_str(a));
      }
      const JsonVal *where = json_get(f->schema, "x-where");
      if (where) json_set(fo, "code", json_clone(where));
      json_set(fo, "moddable", json_bool(!f->readonly));
      json_push(fields, fo);
    }
    json_set(o, "fields", fields);
    json_push(list, o);
  }
  return list;
}

void data_free(DataSet *ds) {
  for (int ti = 0; ti < ds->n; ti++) {
    DataTable *t = &ds->t[ti];
    for (int r = 0; r < t->nrows; r++) {
      free(t->row_id[r]);
      json_free(t->base[r]);
      json_free(t->row[r]);
      free(t->row_mod[r]);
    }
    for (int fi = 0; fi < t->nf; fi++) free(t->f[fi].name);
    free(t->f); free(t->row_id); free(t->base); free(t->row); free(t->row_mod); free(t->touched); free(t->row_touched);
    free(t->name); free(t->path); free(t->title);
    json_free(t->def);
  }
  free(ds->t);
  mdiags_free(&ds->diags);
  memset(ds, 0, sizeof *ds);
}

// ------------------------------------------------------------------ image file

static unsigned char *file_at(void *ctx, uint32_t addr, uint32_t len) {
  DataImageFile *f = (DataImageFile *)ctx;
  if (addr < f->base || addr - f->base > f->size || len > f->size - (addr - f->base)) return NULL;
  return f->bytes + (addr - f->base);
}

int data_image_file_open(DataImageFile *f, const char *path, uint32_t base) {
  size_t len = 0;
  memset(f, 0, sizeof *f);
  f->bytes = (unsigned char *)mods_read_file(path, &len);
  if (!f->bytes) return 0;
  f->base = base;
  f->size = (uint32_t)len;
  return 1;
}

void data_image_file_close(DataImageFile *f) {
  free(f->bytes);
  memset(f, 0, sizeof *f);
}

DataImage data_image_of_file(DataImageFile *f) {
  DataImage im = {file_at, f};
  return im;
}
