// See ui_updates.h.
#include "port/ui/ui_updates.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mods/json.h"

static int next_part(const char **p) {
  while (**p && !isdigit((unsigned char)**p)) (*p)++;
  int v = 0;
  while (isdigit((unsigned char)**p)) v = v * 10 + (*(*p)++ - '0');
  return v;
}

int upd_version_cmp(const char *a, const char *b) {
  if (!a) a = "";
  if (!b) b = "";
  for (int i = 0; i < 3; i++) {
    int x = next_part(&a), y = next_part(&b);
    if (y > x) return 1;
    if (y < x) return -1;
  }
  return 0;
}

static void copy(char *dst, size_t n, const JsonVal *v) {
  snprintf(dst, n, "%s", v && v->type == JS_STR && v->s ? v->s : "");
}

int upd_parse_news(UpdState *s, const char *text, char *err, size_t errlen) {
  JsonVal *root = json_parse(text, err, errlen);
  if (!root) return 0;
  JsonVal *rels = json_get(root, "releases");
  if (!rels || rels->type != JS_ARR) {
    snprintf(err, errlen, "no releases list");
    json_free(root);
    return 0;
  }
  upd_free(s);
  s->rel = (UpdRelease *)calloc((size_t)rels->len + 1, sizeof *s->rel);
  for (int i = 0; s->rel && i < rels->len; i++) {
    const JsonVal *r = rels->items[i];
    if (r->type != JS_OBJ) continue;
    UpdRelease *o = &s->rel[s->n_rel];
    copy(o->version, sizeof o->version, json_get(r, "version"));
    if (!o->version[0]) continue;
    copy(o->date, sizeof o->date, json_get(r, "date"));
    copy(o->name, sizeof o->name, json_get(r, "name"));
    JsonVal *notes = json_get(r, "notes");
    if (notes && notes->type == JS_ARR) {
      o->note = (char **)calloc((size_t)notes->len + 1, sizeof *o->note);
      for (int k = 0; o->note && k < notes->len; k++)
        if (notes->items[k]->type == JS_STR && notes->items[k]->s) o->note[o->n++] = _strdup(notes->items[k]->s);
    }
    s->n_rel++;
  }
  // newest first, whatever order the file has
  for (int a = 1; a < s->n_rel; a++)
    for (int b = a; b > 0 && upd_version_cmp(s->rel[b - 1].version, s->rel[b].version) > 0; b--) {
      UpdRelease t = s->rel[b];
      s->rel[b] = s->rel[b - 1];
      s->rel[b - 1] = t;
    }
  s->expanded = -1;
  json_free(root);
  return 1;
}

int upd_load_news(UpdState *s, const char *path, char *err, size_t errlen) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    snprintf(err, errlen, "cannot open %s", path);
    return 0;
  }
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc((size_t)n + 1);
  if (!buf) {
    fclose(f);
    return 0;
  }
  buf[fread(buf, 1, (size_t)n, f)] = 0;
  fclose(f);
  int ok = upd_parse_news(s, buf, err, errlen);
  free(buf);
  return ok;
}

void upd_free(UpdState *s) {
  for (int i = 0; i < s->n_rel; i++) {
    for (int k = 0; k < s->rel[i].n; k++) free(s->rel[i].note[k]);
    free(s->rel[i].note);
  }
  free(s->rel);
  s->rel = NULL;
  s->n_rel = 0;
}

static int ends_with(const char *s, const char *suf) {
  size_t a = strlen(s), b = strlen(suf);
  return a >= b && !_stricmp(s + a - b, suf);
}

int upd_parse_feed(const char *text, char *version, size_t vlen, char *zip_url, size_t zlen, char *sha_url, size_t slen) {
  char err[96];
  JsonVal *root = json_parse(text, err, sizeof err);
  if (!root) return 0;
  int ok = 0;
  const JsonVal *tag = json_get(root, "tag_name"), *assets = json_get(root, "assets");
  zip_url[0] = sha_url[0] = 0;
  if (tag && tag->type == JS_STR && tag->s && assets && assets->type == JS_ARR) {
    const char *t = tag->s;
    if (*t == 'v' || *t == 'V') t++;
    snprintf(version, vlen, "%s", t);
    for (int i = 0; i < assets->len; i++) {
      const JsonVal *nm = json_get(assets->items[i], "name"), *url = json_get(assets->items[i], "browser_download_url");
      if (!nm || !url || nm->type != JS_STR || url->type != JS_STR || !nm->s || !url->s) continue;
      if (ends_with(nm->s, "-windows.zip")) snprintf(zip_url, zlen, "%s", url->s);
      else if (ends_with(nm->s, "-windows.zip.sha256")) snprintf(sha_url, slen, "%s", url->s);
    }
    ok = version[0] && zip_url[0] && sha_url[0];
  }
  json_free(root);
  return ok;
}
