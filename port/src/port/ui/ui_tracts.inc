// The land purchase map (FUN_004587a0, year end): included by ui_pages.c. Nine tract cards, the map picture and Cash Reserve,
// all read from what golf drew (reports_game.c recorded the tile texts with their places). A card clicks the tile's centre in
// golf's page, so golf's own hit test, its confirm question and the purchase run; "No thanks" clicks golf's check button.

typedef struct {
  int has;          // golf drew this tile
  int sold;         // "Already purchased."
  char text[RP_TEXT];
  char price[RP_TEXT];
} Tract;

// golf's tile k (0..8, numbered k + 1): column k / 3 and row k % 3 of the tile band
static const int TR_X[3] = {140, 400, 656};
static const int TR_Y[3] = {88, 156, 224};

static void tr_read(const RpSnap *s, Tract tr[9], char *cash, char *cash_label) {
  memset(tr, 0, 9 * sizeof *tr);
  for (int i = 0; i < s->n; i++) {
    const RpItem *it = &s->it[i];
    if (it->type != RP_IT_TEXT || !it->s[0] || it->y < 56 || it->y >= 262 || it->align == RP_AL_CENTRE) continue;
    int xa = it->align == RP_AL_RIGHT ? it->x - 0xa5 : it->x;   // a price ends at the tile's text column plus 0xa5
    int col = xa < 200 ? 0 : xa < 470 ? 1 : 2;
    int row = (it->y - 62) / 68;
    if (row < 0) row = 0;
    if (row > 2) row = 2;
    Tract *t = &tr[col * 3 + row];
    t->has = 1;
    if (it->align == RP_AL_RIGHT) fix_text(t->price, sizeof t->price, it->s);
    else if (!strncmp(it->s, "Already", 7)) t->sold = 1;
    else fix_text(t->text, sizeof t->text, it->s);
  }
  const RpItem *a = txt(s, 720, 264), *l = txt(s, 548, 263);
  fix_text(cash, RP_TEXT, a ? a->s : "");
  fix_text(cash_label, RP_TEXT, l ? l->s : "Cash Reserve");
}

#define TR_W 1160.0f
#define TR_H 808.0f

static int draw_tracts(UiCtx *c, const RpSnap *s, SDL_Texture *const *art, RpAction *acts, int max) {
  const UiTokens *t = ui_tokens(c);
  UiDraw *d = ui_drawer(c);
  int na = 0;
  Tract tr[9];
  char cash[RP_TEXT], cash_label[RP_TEXT];
  tr_read(s, tr, cash, cash_label);
  UiRect scr = ui_screen(c);
  float margin = ui_s(c, t->edge_margin);
  float k = fminf(1.0f, fminf((scr.w - 2 * margin) / ui_s(c, TR_W), (scr.h - 2 * margin) / ui_s(c, TR_H)));
  if (k < 1.0f) ui_push_scale(c, k);
  float pad = ui_s(c, 28);
  UiRect pr = ui_rect_round(ui_center(ui_screen(c), ui_s(c, TR_W), ui_s(c, TR_H)));
  scrim_and_panel(c, pr);
  UiRect in = ui_inset_all(pr, pad);
  g_pf_layout.panel = pr;

  UiFont tf = ui_font(c, UI_TYPE_TITLE);
  float cb = ui_s(c, 40);
  UiRect close_r = ui_rect(in.x + in.w - cb, in.y, cb, cb);
  if (ui_icon_button(c, ui_id("pages.close"), close_r, UI_ICON_X, 0, "Close (Esc)") && na < max) acts[na++] = (RpAction){RP_A_CLOSE, 0};
  g_pf_layout.close_x = close_r.x + close_r.w / 2, g_pf_layout.close_y = close_r.y + close_r.h / 2;
  ui_text_fit(c, ui_rect(in.x, in.y + (cb - tf.line) / 2, ui_s(c, 400), tf.line), UI_TYPE_TITLE, t->text_primary, "Tracts for sale", 0);
  // Cash Reserve, top right
  char line[RP_TEXT * 2];
  snprintf(line, sizeof line, "%s  %s", cash_label, cash);
  float cw = ui_text_width(c, UI_TYPE_HEADING, line) + ui_s(c, 36);
  UiRect cr = ui_rect(in.x + in.w - cb - ui_s(c, 16) - cw, in.y, cw, cb);
  uid_rrect(d, cr, cb / 2, uid_alpha(t->surface_inset, 0.7f));
  uid_rrect_stroke(d, cr, cb / 2, ui_s(c, 1), uid_alpha(t->divider, 0.8f));
  ui_text_fit(c, ui_inset(cr, ui_s(c, 18), 0, ui_s(c, 18), 0), UI_TYPE_HEADING, t->text_primary, line, 0.5f);

  // the nine tract cards in reading order (golf numbers its tiles down the columns)
  float gap = ui_s(c, 12), ch = ui_s(c, 86);
  float cwid = (in.w - 2 * gap) / 3;
  float gy = in.y + ui_s(c, 40 + 16);
  UiFont bf = ui_font(c, UI_TYPE_BODY), lf = ui_font(c, UI_TYPE_LABEL);
  for (int n = 0; n < 9; n++) {
    UiRect r = ui_rect(in.x + (n % 3) * (cwid + gap), gy + (n / 3) * (ch + gap), cwid, ch);
    const Tract *tt = &tr[n];
    char key[24];
    snprintf(key, sizeof key, "tr.tract%d", n);
    uint32_t id = ui_id(key);
    int can = tt->has && !tt->sold && tt->text[0];
    ui_hit(c, r, ui_s(c, t->radius_card), id, can ? UI_HIT_BLOCK | UI_HIT_INTERACT : UI_HIT_BLOCK);
    float hov = ui_anim(c, id, can && ui_is_hot(c, id) ? 1.0f : 0.0f, t->motion_hover);
    uid_rrect(d, r, ui_s(c, t->radius_card), uid_alpha(can ? t->surface_inset : t->surface_inset, can ? 0.55f + 0.4f * hov : 0.3f));
    uid_rrect_stroke(d, r, ui_s(c, t->radius_card), ui_s(c, can && hov > 0.5f ? 2 : 1), can && hov > 0.5f ? t->accent : uid_alpha(t->divider, 0.6f));
    float bs = ui_s(c, 44);
    UiRect badge = ui_rect(r.x + ui_s(c, 12), r.y + (r.h - bs) / 2, bs, bs);
    uid_rrect(d, badge, bs / 2, can ? uid_mix(t->accent_soft, t->accent, 0.2f * hov) : uid_alpha(t->divider, 0.5f));
    char num[8];
    snprintf(num, sizeof num, "%d", n + 1);
    ui_text_in(c, badge, UI_TYPE_HEADING, can ? t->accent_soft_text : t->text_tertiary, num, 0.5f);
    float tx = badge.x + bs + ui_s(c, 12), tw = r.x + r.w - tx - ui_s(c, 12);
    if (tt->sold) {
      ui_text_in(c, ui_rect(tx, r.y, tw, r.h), UI_TYPE_BODY, t->text_tertiary, "Already purchased", 0);
    } else if (tt->text[0]) {
      char lines[WRAP_LINES][WRAP_LEN];
      int nl = wrap(c, UI_TYPE_BODY, tt->text, tw, lines);
      if (nl > 3) nl = 3;
      float ty = r.y + ui_s(c, 8);
      for (int i = 0; i < nl; i++, ty += bf.line) ui_text_in(c, ui_rect(tx, ty, tw, bf.line), UI_TYPE_BODY, t->text_primary, lines[i], 0);
      if (tt->price[0]) ui_text_in(c, ui_rect(tx, r.y + r.h - lf.line - ui_s(c, 6), tw, lf.line), UI_TYPE_LABEL, t->accent_soft_text, tt->price, 0);
    }
    if (can && ui_clicked(c, id) && na < max) acts[na++] = (RpAction){RP_A_CLICK, CLICK(TR_X[n / 3], TR_Y[n % 3])};
  }

  // the map, with golf's painted frame
  float my = gy + 3 * ch + 2 * gap + ui_s(c, 14);
  float fy = in.y + in.h - ui_s(c, 44);
  UiRect M = ui_rect(in.x, my, in.w, fy - ui_s(c, 14) - my);
  int pic = 0, pw = 0, ph = 0;
  for (int i = 0; i < s->n; i++)
    if (s->it[i].type == RP_IT_PIC) pic = s->it[i].art, pw = s->it[i].w, ph = s->it[i].h;
  SDL_Texture *ptex = art && pic > 0 ? art[pic - 1] : NULL;
  uid_rrect(d, M, ui_s(c, t->radius_card), uid_alpha(t->surface_inset, 0.35f));
  if (ptex && pw > 0 && ph > 0) {
    float kk = fminf((M.w - ui_s(c, 12)) / pw, (M.h - ui_s(c, 12)) / ph);
    UiRect ir = ui_rect_round(ui_center(M, pw * kk, ph * kk));
    uid_image(d, ptex, NULL, ir, 0, (UiColor){255, 255, 255, 255});
    // the check button painted in golf's frame (page 662..725 x 533..596) is golf's own "leave" control: keep it working
    UiRect ck = ui_rect(ir.x + (662 - 8) * kk, ir.y + (533 - 282) * kk, 64 * kk, 64 * kk);
    uint32_t cid = ui_id("tr.check");
    ui_hit(c, ck, ck.w / 2, cid, UI_HIT_BLOCK | UI_HIT_INTERACT);
    float chov = ui_anim(c, cid, ui_is_hot(c, cid) ? 1.0f : 0.0f, t->motion_hover);
    if (chov > 0.01f) uid_rrect_stroke(d, ck, ck.w / 2, ui_s(c, 2), uid_alpha(t->accent, chov));
    if (ui_clicked(c, cid) && na < max) acts[na++] = (RpAction){RP_A_CLICK, CLICK(688, 560)};
  }

  // footer
  float bh = ui_s(c, 44);
  const char *lab = "I don't think I'll buy any land";
  float bw = ui_text_width(c, UI_TYPE_LABEL, lab) + ui_s(c, 64);
  UiRect br = ui_rect(in.x + in.w - bw, fy, bw, bh);
  if (ui_button(c, ui_id("tr.no"), br, lab, UI_ICON_NONE, UI_BTN_PRIMARY, 0) && na < max) acts[na++] = (RpAction){RP_A_CLICK, CLICK(688, 560)};
  ui_text_in(c, ui_rect(in.x, fy, in.w - bw - ui_s(c, 12), bh), UI_TYPE_CAPTION, t->text_tertiary, "Pick a tract to buy it. Esc leaves without buying.", 0);
  if (k < 1.0f) ui_pop_scale(c);
  return na;
}
