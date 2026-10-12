// UI font switch (card task-25). See ui_font.h.
#define _GNU_SOURCE
#include "port/ui_font.h"

#ifndef __EMSCRIPTEN__
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "mods/mods.h"
#include "port/runtime.h"

int uif_enabled(void) {
  static unsigned seen = ~0u;
  static int on;
  unsigned s = mods_serial();
  if (s != seen) {
    seen = s;
    const char *v = mods_get("ui.font");
    on = v && (!strcasecmp(v, "plus_jakarta") || !strcasecmp(v, "plus-jakarta") || !strcasecmp(v, "plusjakarta"));
  }
  return on;
}

// ---------------------------------------------------------------- font_map.txt
//
//   fit 0.98                  sample-sentence width as a fraction of the original face
//   tabular 1                 tabular digits (tnum)
//   face <substr> <w> <bold>  first rule whose substring is in the face name wins;
//                             <w> is the weight, <bold> the weight when the game asks
//                             for bold (lfWeight >= 600 or "bold" in the face name)

#define MAX_RULES 24
static struct { char match[48]; int w, bold; } g_rules[MAX_RULES];
static int g_nrules, g_loaded, g_ok;
static float g_fit = 0.98f;
static int g_tabular = 1;

int uif_load_map(void) {
  if (g_loaded) return g_ok;
  g_loaded = 1;
  FILE *f = fopen(port_host_path("/fonts/plus-jakarta-sans/font_map.txt"), "rb");
  if (!f) {
    fprintf(stderr, "ui.font: fonts/plus-jakarta-sans/font_map.txt not found, keeping the original fonts\n");
    return 0;
  }
  char line[256];
  while (fgets(line, sizeof line, f)) {
    char *h = strchr(line, '#');
    if (h) *h = 0;
    char key[32], m[48];
    float v;
    int a, b;
    if (sscanf(line, "%31s", key) != 1) continue;
    if (!strcmp(key, "fit") && sscanf(line, "%*s %f", &v) == 1) g_fit = v;
    else if (!strcmp(key, "tabular") && sscanf(line, "%*s %d", &a) == 1) g_tabular = a;
    else if (!strcmp(key, "face") && sscanf(line, "%*s %47s %d %d", m, &a, &b) == 3 && g_nrules < MAX_RULES) {
      snprintf(g_rules[g_nrules].match, sizeof g_rules[0].match, "%s", m);
      g_rules[g_nrules].w = a;
      g_rules[g_nrules].bold = b;
      g_nrules++;
    }
  }
  fclose(f);
  g_ok = g_nrules > 0;
  return g_ok;
}

int uif_style(const char *face, int lf_weight, int *weight) {
  if (!uif_load_map()) return 0;
  int bold = lf_weight >= 600 || strcasestr(face, "bold") != NULL;
  for (int i = 0; i < g_nrules; i++) {
    if (!strcmp(g_rules[i].match, "*") || strcasestr(face, g_rules[i].match)) {
      *weight = bold ? g_rules[i].bold : g_rules[i].w;
      return 1;
    }
  }
  return 0;
}

float uif_fit(void) { return g_fit; }
int uif_tabular(void) { return g_tabular; }

// ---------------------------------------------------------------- blending

#define GAMMA 1.8f
static float g_lin[256];
static uint8_t g_back[4096];
static int g_tables;

static void tables(void) {
  for (int i = 0; i < 256; i++) g_lin[i] = powf(i / 255.0f, GAMMA);
  for (int i = 0; i < 4096; i++) g_back[i] = (uint8_t)(powf(i / 4095.0f, 1.0f / GAMMA) * 255.0f + 0.5f);
  g_tables = 1;
}

uint8_t uif_blend_chan(int dst, int src, int cov) {
  if (!g_tables) tables();
  float a = cov / 255.0f;
  float l = g_lin[dst] * (1.0f - a) + g_lin[src] * a;
  int i = (int)(l * 4095.0f + 0.5f);
  return g_back[i > 4095 ? 4095 : i];
}
#endif
