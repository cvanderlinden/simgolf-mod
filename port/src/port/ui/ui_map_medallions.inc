// Hand-authored bounded geometry. Original property tag ids, independent of price order and biome sprite.
static void map_medallion(UiCtx *c, UiRect r, int property, int muted) {
  static const UiColor palette[16] = {
    {74,133,118,255},{73,142,163,255},{112,135,157,255},{169,137,109,255},
    {176,144,116,255},{82,146,132,255},{108,154,130,255},{111,140,161,255},
    {124,145,116,255},{102,144,125,255},{99,144,107,255},{99,128,138,255},
    {140,145,113,255},{156,135,112,255},{112,161,153,255},{105,152,135,255}};
  if (property < 0 || property > 15) return;
  UiDraw *d = ui_drawer(c);
  UiColor ink = palette[property], cream = {249,247,235,255}, line = {235,242,226,255};
  if (muted) ink = uid_mix(ink, (UiColor){166,173,168,255}, 0.45f);
  float u = r.w / 40, x = r.x, y = r.y;
  uid_rrect(d, r, 11*u, cream);
  uid_rrect(d, ui_inset_all(r, 2*u), 9*u, ink);
  uid_rrect_stroke(d, r, 11*u, u, uid_alpha(ink, 0.35f));
  // Coast, hills and a distinct central local motif share one quiet thumbnail grammar.
  uid_line(d,x+6*u,y+30*u,x+34*u,y+30*u,1.5f*u,uid_alpha(line,0.65f));
  switch (property) {
    case 0: // Monterey cypress
      uid_line(d,x+21*u,y+29*u,x+21*u,y+15*u,2*u,line);
      uid_rrect(d,ui_rect(x+8*u,y+9*u,25*u,10*u),5*u,line); break;
    case 1: // San Diego sail and coast
      uid_triangle(d,x+20*u,y+8*u,x+20*u,y+25*u,x+9*u,y+25*u,line);
      uid_triangle(d,x+23*u,y+12*u,x+31*u,y+25*u,x+23*u,y+25*u,uid_alpha(line,0.75f)); break;
    case 2: // Rocky Mountains, twin snowy peaks
      uid_triangle(d,x+7*u,y+29*u,x+18*u,y+10*u,x+26*u,y+29*u,line);
      uid_triangle(d,x+21*u,y+29*u,x+29*u,y+16*u,x+35*u,y+29*u,uid_alpha(line,0.7f)); break;
    case 3: // Las Vegas diamond
      uid_triangle(d,x+20*u,y+7*u,x+9*u,y+19*u,x+31*u,y+19*u,line);
      uid_triangle(d,x+9*u,y+19*u,x+20*u,y+29*u,x+31*u,y+19*u,line); break;
    case 4: // Phoenix cactus and sun
      uid_line(d,x+21*u,y+29*u,x+21*u,y+11*u,3*u,line);
      uid_line(d,x+12*u,y+17*u,x+12*u,y+22*u,3*u,line);
      uid_line(d,x+12*u,y+22*u,x+21*u,y+22*u,3*u,line);
      uid_line(d,x+29*u,y+14*u,x+29*u,y+19*u,3*u,line);
      uid_line(d,x+21*u,y+19*u,x+29*u,y+19*u,3*u,line); break;
    case 5: // Hawaii waterfall
      uid_triangle(d,x+7*u,y+29*u,x+18*u,y+10*u,x+34*u,y+29*u,uid_alpha(line,0.65f));
      uid_line(d,x+20*u,y+15*u,x+20*u,y+29*u,4*u,cream); break;
    case 6: // Oahu torii garden gate
      uid_line(d,x+9*u,y+12*u,x+31*u,y+12*u,3*u,line);
      uid_line(d,x+12*u,y+17*u,x+28*u,y+17*u,2*u,line);
      uid_line(d,x+14*u,y+12*u,x+14*u,y+29*u,3*u,line);
      uid_line(d,x+26*u,y+12*u,x+26*u,y+29*u,3*u,line); break;
    case 7: // Nova Scotia lighthouse
      uid_triangle(d,x+16*u,y+28*u,x+20*u,y+10*u,x+24*u,y+28*u,line);
      uid_rrect(d,ui_rect(x+15*u,y+9*u,10*u,6*u),u,line);
      uid_line(d,x+9*u,y+12*u,x+13*u,y+12*u,u,line); break;
    case 8: // Northeast covered bridge
      uid_rrect(d,ui_rect(x+8*u,y+15*u,24*u,11*u),2*u,line);
      uid_triangle(d,x+6*u,y+15*u,x+20*u,y+7*u,x+34*u,y+15*u,line);
      uid_rrect(d,ui_rect(x+16*u,y+19*u,8*u,10*u),2*u,ink); break;
    case 9: // Carolina putting green
      uid_rrect(d,ui_rect(x+8*u,y+24*u,24*u,7*u),3.5f*u,line);
      uid_line(d,x+21*u,y+26*u,x+21*u,y+9*u,1.5f*u,line);
      uid_triangle(d,x+21*u,y+9*u,x+31*u,y+13*u,x+21*u,y+17*u,line); break;
    case 10: // Ireland shamrock
      for(int k=0;k<3;k++){float a=2.0944f*k-1.5708f; uid_rrect(d,ui_rect(x+(20+6*cosf(a)-5)*u,y+(19+6*sinf(a)-5)*u,10*u,10*u),5*u,line);}
      uid_line(d,x+20*u,y+20*u,x+18*u,y+29*u,2*u,line); break;
    case 11: // Scotland castle
      uid_rrect(d,ui_rect(x+10*u,y+13*u,20*u,16*u),u,line);
      for(int k=0;k<3;k++)uid_rrect(d,ui_rect(x+(10+k*8)*u,y+9*u,4*u,8*u),u,line);
      uid_rrect(d,ui_rect(x+17*u,y+22*u,6*u,8*u),3*u,ink); break;
    case 12: // Wales standing stones
      uid_rrect(d,ui_rect(x+10*u,y+13*u,6*u,16*u),2*u,line);
      uid_rrect(d,ui_rect(x+24*u,y+12*u,6*u,17*u),2*u,line);
      uid_rrect(d,ui_rect(x+8*u,y+9*u,24*u,6*u),2*u,line); break;
    case 13: // Spain vineyard
      for(int k=0;k<5;k++){float xx=16+(k%2)*8, yy=12+(k/2)*6;uid_rrect(d,ui_rect(x+(xx-3)*u,y+(yy-3)*u,6*u,6*u),3*u,line);}
      uid_line(d,x+20*u,y+8*u,x+26*u,y+6*u,2*u,line); break;
    case 14: // Florida palm
      uid_line(d,x+19*u,y+29*u,x+21*u,y+14*u,2*u,line);
      for(int k=0;k<5;k++){float a=3.14159f+k*.7854f;uid_line(d,x+21*u,y+14*u,x+(21+11*cosf(a))*u,y+(14+8*sinf(a))*u,2*u,line);} break;
    case 15: // Jamaica sun over hills
      uid_rrect(d,ui_rect(x+21*u,y+8*u,10*u,10*u),5*u,line);
      uid_triangle(d,x+7*u,y+29*u,x+16*u,y+17*u,x+27*u,y+29*u,line);
      uid_triangle(d,x+21*u,y+29*u,x+28*u,y+21*u,x+35*u,y+29*u,uid_alpha(line,0.7f)); break;
  }
}
