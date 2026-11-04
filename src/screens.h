// ===== screen-specific handlers added in the fifth session =====
static int bdetail_tap(u64 g,float x,float y);
static int help_text_is(u64 g,const char*t);
static void press(u64 b){ q_push(b,2,4); }
static u64 hand_owner_vt(u64 g,u64*po){ u64 o=RD64(g+G_HAND_OWNER); if(po) *po=o; if(!mem_ok(o)) return 0; return RD64(o)-g_main; }
// Bottom-right button prompts (row at y ~700). Each entry: x range and the button it stands for.
typedef struct { float x0,x1; u64 btn; } Prompt;
static int prompt_tap(float x,float y,const Prompt*p,int n){
  if(y<682.f||y>718.f) return 0;
  for(int i=0;i<n;i++) if(x>=p[i].x0&&x<=p[i].x1){ press(p[i].btn); tm_log2("prompt",(s64)p[i].btn,(s64)x); return 1; }
  return 0; }
#define NP(a) ((int)(sizeof(a)/sizeof(a[0])))

// ---- Calendar (Base of Operations) ----
// Main screen: hand list Explore/Seminar/Battle/Rest (vt 0x1a07bd8), the month calendar on the right (drawn 47.7 px left of
// its position in schedule mode; day rows from y 193, under the weekday names) and prompts L Unit Appearance / - Statistics / X Menu /
// Right Check Schedule.
// Schedule mode (Right): hand owner vt 0x1a249f8, selected day (1-based) at owner+0x88, month at owner+0xa4.
// Day cells (720p): Mon..Sat 80.3 wide from x 569.6, Sun 1051.6-1213.2; rows 66.67 apart. The hand sits 20.4 px right of a
// cell's left edge (Sun: hand x 1096.7) and 17.4 px below its top.
#define VT_CAL_MENU 0x1a07bd8
#define VT_CAL_DAYS 0x1a249f8
#define VT_CAL_POP 0x1a3be68   // A on a day: popup ("View Schedule Details", ...)
#define VT_CAL_EVT 0x1a24bc0   // schedule details: event list (G+0x9b478), row 0 = header, rows 48 apart
static int cal_ev_n;   // a tap on the events panel is waiting for the popup
static const Prompt cal_prompts[]={ {553.f,726.f,BTN_LB}, {775.f,899.f,BTN_MINUS}, {945.f,1050.f,BTN_X}, {1096.f,1264.f,BTN_R} };
static int cal_col_at(float x){ if(x>=569.6f&&x<1051.6f) return (int)((x-569.6f)/80.3f); if(x>=1051.6f&&x<=1213.2f) return 6; return -1; }
// Days of the shown month: the day records at *(owner+0x80) + k*0x188 (k = 1..31) hold (day << 8) | month.
static int cal_days_in(u64 o){ u64 p=RD64(o+0x80); if(!mem_ok(p)||!mem_ok(p+32*0x188)) return 30; int m=*(volatile u8*)(p+0x188), n=0;
  for(int k=1;k<=31;k++){ u64 r=p+(u64)k*0x188; if(*(volatile u8*)r!=m || *(volatile u8*)(r+1)!=k) break; n=k; }
  return n>=28 ? n : 30; }
// Events panel rows (list vt 0x1a24bc0): section headers ("This Week's Activity", "Events") are rows too; byte +0x516+i = row i can be
// selected (0 = header). Hand y of row i (unscrolled) = 149.3 + 48 per row above it + 8 more per header above it (headers are 56 tall).
// Returns the selectable row nearest to y, -1 for none, -2 when the list is scrolled (positions unknown).
static int cal_ev_row(u64 ev,float y){ s32 cnt=RDS32(ev+0x54); if(cnt<=1||cnt>40) return -1; if((short)(RD32(ev+0x68)>>16)!=0) return -2;
  float ry=149.3f, bd=1e9f; int best=-1;
  for(int i=0;i<cnt;i++){ int sel=*(volatile u8*)(ev+0x516+i)!=0; if(sel){ float d=y-ry; if(d<0) d=-d; if(d<bd){ bd=d; best=i; } } ry+= sel ? 48.f : 56.f; }
  return bd<=30.f ? best : -1; }
static int cal_fw=-1, cal_fw_m=-1; static float cal_top;   // days layout seen last (weekday of the 1st, top of the grid), per month
static int cal_month(u64 dl){ u64 p=RD64(dl+0x80); if(!mem_ok(p)||!mem_ok(p+0x188)) return -2; return *(volatile u8*)(p+0x188); }
static int calendar_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0;
  u64 o; u64 vt=hand_owner_vt(g,&o);
  if(vt==VT_CAL_MENU){
    if(prompt_tap(x,y,cal_prompts,NP(cal_prompts))) return 1;
    if(cal_col_at(x+47.7f)>=0 && y>=190.f && y<=640.f){ press(BTN_R); retap_on_change(x+47.7f,y,30); rt_settle=1; tm_log2("calendar open",(s64)x,(s64)y); return 1; }   // replayed once the days have the hand and it stopped (a fixed 30 polls was slow; right away the step got lost while the calendar slid)
    return 0; }                                   // rows: hprof "calendar"
  if(vt==VT_CAL_POP && cal_ev_n>0){              // event-panel tap on its way: "View Schedule Details" (single item) -> A
    cal_ev_n=0; s32 cnt=RDS32(o+0x54); if(cnt==1){ press(BTN_A); if(rt_depth<2) retap_on_change(x,y,40); } tm_log2("calendar popup",cnt,0); return 1; }
  if(vt==VT_CAL_EVT){                            // schedule details: the day's events on the left, X Details toggles the description
    cal_ev_n=0;
    if(inbox(x,y,1150.f,1264.f,682.f,718.f)){ press(BTN_X); return 1; }
    if(x>=560.f){ if(cal_col_at(x)<0 || y<150.f || y>640.f) return 1;          // a day on the calendar
      // instant: back to the days mode the way B does it (mode 2 + the focus switch), then select the day with the layout seen the
      // last time the hand was on the days (weekday of the 1st, top of the grid; kept per month)
      u64 C=o-0xbc8, dl=C+0x500;
      if(mem_ok(C) && RD64(C)==g_main+0x1a04150 && RDS32(C+0x4bf8)==3 && is_widget(dl,VT_CAL_DAYS) && cal_fw>=0 && cal_month(dl)==cal_fw_m){
        int c=cal_col_at(x); if(y<cal_top) return 1; int r=(int)((y-cal_top)/66.67f); int t=r*7+c-cal_fw+1; if(t<1||t>cal_days_in(dl)) return 1;
        *(volatile s32*)(C+0x4bf8)=2; ((void(*)(u64))(g_main+0x304cb0))(C);
        s32 d=RDS32(dl+0x88); g_learn_hold=20;
        if(t!=d){ if(t>1){ *(volatile s32*)(dl+0x88)=t-1; press(BTN_R); } else { *(volatile s32*)(dl+0x88)=2; press(BTN_L); } }
        dt_arm(dl+0x88,t,0,0); tm_log2("calendar day instant",t,d); return 1; }
      if(rt_depth==0){ press(BTN_B); retap_later(x,y,20); } return 1; }   // layout not known yet: back to the days, then the tap
    s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); if(cnt<=1||cnt>40||idx<0||idx>=cnt) return 1;
    if(x<35.f||x>520.f) return 1;
    int t=cal_ev_row(o,y);
    if(t==-2){ float hy=RDF(g+0x30)/1.5f, f=(y-hy)/48.f; int dd=(int)(f+(f>=0?0.5f:-0.5f)); t=idx+dd; if(t<0||t>=cnt||!*(volatile u8*)(o+0x516+t)) return 1; }   // scrolled list: from the hand
    if(t<0) return 1;
    if(t!=idx) list_jump(o,t,0); tm_log2("calendar event",t,idx); return 1; }
  if(vt!=VT_CAL_DAYS) return 0;
  { s32 d0=RDS32(o+0x88); float hx0=RDF(g+0x2c)/1.5f, hy0=RDF(g+0x30)/1.5f; int cs0= hx0>1040.f ? 6 : (int)((hx0-590.f)/80.f+0.5f);
    if(d0>=1 && d0<=31 && cs0>=0 && cs0<=6){ cal_fw=((cs0-(d0-1))%7+7)%7; cal_top=hy0-17.4f-66.67f*(float)((cal_fw+d0-1)/7); cal_fw_m=cal_month(o); } }   // layout cache
  // "L Finish Exploring" (free days: the calendar was opened from the pause book, whose Calendar row stays selected)
  if(inbox(x,y,1090.f,1264.f,682.f,718.f)){ u64 bk=g+0x8def8; if(is_list(bk) && RD64(bk)==g_main+0x1a48738 && RDS32(bk+0x4c)==9) press(BTN_LB); return 1; }
  // the events panel on the left: open the selected day's schedule details (A, then "View Schedule Details")
  // Instant: the schedule controller C (vt 0x1a04150; days list C+0x500, popup C+0x598, events list C+0xbc8) has its mode at
  // C+0x4bf8 (2 days, 3 events). "View Schedule Details" only sets 3 and calls main+0x304cb0(C), which moves the focus between the
  // two lists (the events list already holds the selected day's events). Do that directly, then put the hand on the tapped row.
  if(inbox(x,y,35.f,520.f,150.f,640.f)){ u64 C=o-0x500, ev=C+0xbc8;
    if(mem_ok(C) && RD64(C)==g_main+0x1a04150 && RDS32(C+0x4bf8)==2 && is_widget(ev,VT_CAL_EVT)){
      int t=cal_ev_row(ev,y); if(t<0) return 1;                                   // no events that day, or a header / empty spot
      *(volatile s32*)(C+0x4bf8)=3; ((void(*)(u64))(g_main+0x304cb0))(C);
      if(RDS32(ev+0x4c)!=t) list_jump(ev,t,0); tm_log2("calendar events instant",t,RDS32(ev+0x54)); return 1; }
    if(rt_depth==0){ press(BTN_A); cal_ev_n=1; retap_on_change(x,y,40); tm_log2("calendar events",(s64)x,(s64)y); } else cal_ev_n=0; return 1; }
  s32 d=RDS32(o+0x88); int nd=cal_days_in(o); if(d<1||d>31) return 1;
  float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f;
  int cs= hx>1040.f ? 6 : (int)((hx-590.f)/80.f+0.5f); if(cs<0||cs>6) return 1;
  int fw=((cs-(d-1))%7+7)%7, rs=(fw+d-1)/7;              // weekday of the 1st, row of the selected day
  float top=hy-17.4f-66.67f*(float)rs;                    // top of the first row
  int c=cal_col_at(x); if(c<0||y<top) return 1;
  int r=(int)((y-top)/66.67f); int t=r*7+c-fw+1; if(t<1||t>nd) return 1;
  // a day: the first tap only selects it (also the day that is already selected, e.g. right after the calendar opened: the events
  // panel on the left is tappable anyway), a second tap on it confirms (A: the day's popup)
  if(t==d){ if(dt_same(o+0x88,t,0,0)){ dt_on=0; press(BTN_A); } else dt_arm(o+0x88,t,0,0); tm_log2("calendar day same",t,0); return 1; }
  g_learn_hold=20;
  if(t>1){ *(volatile s32*)(o+0x88)=t-1; press(BTN_R); } else { *(volatile s32*)(o+0x88)=2; press(BTN_L); }
  dt_arm(o+0x88,t,0,0); tm_log2("calendar day",t,d); return 1; }

// ---- Roster (book -> Roster): card wheel, hand owner vt 0x1a09260; index owner+0x9c, unit count owner+0x190 ----
// Up/Down arrows above/below the centre card, the cards stacked above (previous units) and below (next units), centre = A.
static const Prompt roster_prompts[]={ {1001.f,1090.f,BTN_B}, {1139.f,1264.f,BTN_A} };
static int roster_tap(u64 g,float x,float y){
  u64 o; if(hand_owner_vt(g,&o)!=0x1a09260 || !hand_on(g)) return 0;
  s32 i=RDS32(o+0x9c), n=RDS32(o+0x190); if(n<=0||n>60||i<0||i>=n) return 1;
  if(prompt_tap(x,y,roster_prompts,NP(roster_prompts))) return 1;
  int st=0;
  if(inbox(x,y,855.f,940.f,130.f,212.f)) st=-1;           // up arrow
  else if(inbox(x,y,855.f,940.f,535.f,615.f)) st=1;       // down arrow
  else if(x>=1105.f && y<228.f) st= y>=145.f?-1 : y>=101.f?-2 : y>=62.f?-3 : y>=28.f?-4 : -5;
  else if(x>=1105.f && y>=483.f && y<=680.f) st= y<534.f?1 : y<596.f?2 : y<648.f?3 : 4;
  else if(inbox(x,y,835.f,1283.f,220.f,532.f)){ press(BTN_A); return 1; }
  if(!st) return 1;
  if(st==-1){ press(BTN_U); return 1; } if(st==1){ press(BTN_D); return 1; }
  int t=((i+st)%n+n)%n; *(volatile s32*)(o+0x9c)=(t-1+n)%n; press(BTN_D); tm_log2("roster",t,i); return 1; }

// ---- Unit status sheet (Roster -> A, Items X Status, ...): G+0x568f8 (vt 0x1a458f8); tab G+0x5d060 (0..4, wraps) ----
#define G_STATUS 0x568f8
#define G_STATUS_TAB 0x5d060
static const Prompt status_prompts[]={ {836.f,947.f,BTN_X}, {996.f,1113.f,BTN_A}, {1166.f,1264.f,BTN_B} };
// ---- Status sheet Details mode: instant item selection ----
// Focus between the sheet's item lists is a small navigation graph: nodes of 0x30 bytes at G+0x60178 {list, up, down,
// left, right, flags}, the current node pointer at G+0x602c8. The focused list has +8 bits 0x0c set, G+0x58 = list.
// Moving the focus = clear the bits on the old list, set them on the new one, update G+0x58 and the node pointer, then
// SetColumn/SetIndex; the game redraws the hand and the help popup itself. Nodes of the other tabs stay in the table, so
// only nodes reachable from the current one count. Item positions (hand x,y in 720p) were measured per list (G offset).
typedef struct { u32 off; u8 r,c; float x,y,w,hh; } StItem;
static const StItem st_items[]={
  {0x56e90,0,0,368.7f,295.3f,60.0f,25.0f},
  {0x56e90,1,0,72.7f,368.7f,270.0f,16.7f},
  {0x56e90,2,0,72.7f,402.0f,270.0f,16.7f},
  {0x56e90,3,0,72.7f,435.3f,148.6f,16.7f},
  {0x56e90,3,1,251.3f,435.3f,80.0f,16.7f},
  {0x56e90,4,0,72.7f,468.7f,148.6f,16.7f},
  {0x56e90,4,1,251.3f,468.7f,80.0f,16.7f},
  {0x576a0,0,0,81.3f,543.3f,148.7f,16.7f},
  {0x576a0,0,1,260.0f,543.3f,150.0f,16.7f},
  {0x576a0,1,0,81.3f,576.6f,148.7f,16.7f},
  {0x576a0,1,1,260.0f,576.6f,150.0f,16.7f},
  {0x576a0,2,0,81.3f,610.0f,148.7f,16.7f},
  {0x576a0,2,1,260.0f,610.0f,150.0f,16.7f},
  {0x576a0,3,0,81.3f,643.3f,148.7f,16.7f},
  {0x576a0,3,1,260.0f,643.3f,150.0f,16.7f},
  {0x57d48,0,0,457.3f,159.3f,360.0f,20.0f},
  {0x57d48,1,0,457.3f,199.3f,360.0f,20.0f},
  {0x583f0,0,0,458.7f,284.7f,360.0f,17.3f},
  {0x583f0,1,0,458.7f,319.4f,360.0f,17.3f},
  {0x583f0,2,0,458.7f,354.0f,360.0f,17.3f},
  {0x583f0,3,0,458.7f,388.7f,360.0f,17.3f},
  {0x583f0,4,0,458.7f,423.4f,360.0f,17.3f},
  {0x583f0,5,0,458.7f,458.1f,360.0f,17.3f},
  {0x58a98,0,0,458.7f,540.7f,360.0f,17.3f},
  {0x58a98,1,0,458.7f,575.3f,360.0f,17.3f},
  {0x58a98,2,0,458.7f,610.0f,360.0f,17.4f},
  {0x58a98,3,0,458.7f,654.0f,360.0f,22.0f},
  {0x59140,0,0,856.0f,166.7f,360.0f,25.0f},
  {0x59140,1,0,856.0f,220.0f,360.0f,17.3f},
  {0x59140,2,0,856.0f,254.7f,360.0f,17.3f},
  {0x59140,3,0,856.0f,289.3f,360.0f,17.3f},
  {0x59140,4,0,856.0f,343.3f,360.0f,17.3f},
  {0x59140,5,0,856.0f,378.0f,360.0f,17.3f},
  {0x59140,6,0,856.0f,412.7f,360.0f,17.3f},
  {0x59140,7,0,856.0f,447.3f,360.0f,17.3f},
  {0x59140,8,0,856.0f,482.0f,360.0f,17.3f},
  {0x597e8,0,0,856.0f,575.3f,330.0f,20.4f},
  {0x597e8,1,0,875.3f,616.0f,148.0f,16.4f},
  {0x597e8,1,1,1053.3f,616.0f,150.0f,16.4f},
  {0x597e8,2,0,870.7f,648.7f,330.0f,16.4f},
  {0x59e90,0,0,458.7f,160.7f,360.0f,17.3f},
  {0x59e90,1,0,458.7f,195.4f,360.0f,17.3f},
  {0x59e90,2,0,458.7f,230.0f,360.0f,17.3f},
  {0x59e90,3,0,458.7f,264.7f,360.0f,17.3f},
  {0x59e90,4,0,458.7f,299.4f,360.0f,17.3f},
  {0x59e90,5,0,458.7f,334.1f,360.0f,17.3f},
  {0x59e90,6,0,458.7f,368.7f,360.0f,17.3f},
  {0x59e90,7,0,458.7f,403.4f,360.0f,17.3f},
  {0x59e90,8,0,458.7f,438.1f,360.0f,17.3f},
  {0x59e90,9,0,458.7f,472.7f,360.0f,17.3f},
  {0x5a538,0,0,458.7f,563.3f,360.0f,17.3f},
  {0x5a538,1,0,458.7f,598.0f,360.0f,17.3f},
  {0x5a538,2,0,458.7f,632.6f,360.0f,17.3f},
  {0x5abe0,0,0,796.7f,166.7f,420.0f,23.3f},
  {0x5abe0,1,0,796.7f,213.4f,420.0f,23.3f},
  {0x5abe0,2,0,796.7f,260.0f,420.0f,23.3f},
  {0x5abe0,3,0,796.7f,306.7f,420.0f,23.3f},
  {0x5abe0,4,0,796.7f,353.4f,420.0f,23.3f},
  {0x5abe0,5,0,796.7f,400.1f,420.0f,23.3f},
  {0x5abe0,6,0,796.7f,446.7f,420.0f,23.3f},
  {0x5abe0,7,0,796.7f,493.4f,420.0f,23.3f},
  {0x5abe0,8,0,796.7f,540.1f,420.0f,23.3f},
  {0x5abe0,9,0,796.7f,586.7f,420.0f,23.3f},
  {0x5abe0,10,0,796.7f,633.4f,420.0f,23.3f},
};

#define G_STNAV 0x60178
#define G_STCUR 0x602c8
static int st_hit_list(u64 g,u64 L,float x,float y,int*orow,int*ocol){
  u32 off=(u32)(L-g); s32 cnt=RDS32(L+0x54), ccnt=RDS32(L+0x58); if(cnt<=0||cnt>40) return 0; if(ccnt<1) ccnt=1;
  if(off==0x5b288||off==0x5bde8){                   // Classes (1)/(2): grid, column 0 = row header, per-row column counts at +0x53c
    for(int r=0;r<cnt&&r<8;r++){ int nc=(int)*(volatile signed char*)(L+0x53c+r); if(nc>ccnt) nc=ccnt;
      for(int c=1;c<nc;c++){ float hx=620.f+100.f*(float)(c-1), hy=164.f+100.f*(float)r;
        if(x>=hx-25.f&&x<=hx+75.f&&y>=hy-48.f&&y<=hy+42.f){ *orow=r; *ocol=c; return 1; } } }
    // category headers (Unique / Beginner / Intermediate ...) in column 0; a category spanning two rows has a header cell on each
    if(x>=448.f&&x<=588.f&&y>=106.f){ int r=(int)((y-106.f)/100.f); if(r>=0&&r<cnt&&r<8){ *orow=r; *ocol=0; return 1; } }
    return 0; }
  for(unsigned i=0;i<sizeof(st_items)/sizeof(st_items[0]);i++){ const StItem*e=&st_items[i]; if(e->off!=off) continue;
    if(e->r>=cnt || e->c>=ccnt) continue;
    float dy=y-e->y; if(dy<0) dy=-dy; if(dy>e->hh || x<e->x-25.f || x>e->x+e->w) continue;
    *orow=e->r; *ocol=e->c; return 1; }
  return 0; }
static int status_instant(u64 g,float x,float y){
  u64 cur=RD64(g+G_STCUR), nb=g+G_STNAV, ne=g+G_STCUR; if(cur<nb||cur>=ne||((cur-nb)%0x30)) return 0;
  int nn=(int)((ne-nb)/0x30); if(nn>8) nn=8;
  u8 seen[8]={0}; int st[8], sp=0; int k0=(int)((cur-nb)/0x30); seen[k0]=1; st[sp++]=k0;
  while(sp){ int k=st[--sp]; u64 nd=nb+(u64)k*0x30;
    for(int d=1;d<=4;d++){ u64 p=RD64(nd+(u64)d*8); if(!p) continue;
      for(int j=0;j<nn;j++) if(!seen[j] && RD64(nb+(u64)j*0x30)==p){ seen[j]=1; st[sp++]=j; } } }
  for(int k=0;k<nn;k++){ if(!seen[k]) continue; u64 nd=nb+(u64)k*0x30, L=RD64(nd); if(!mem_ok(L)) continue;
    int r,c; if(!st_hit_list(g,L,x,y,&r,&c)) continue;
    u64 old=RD64(g+G_HAND_OWNER);
    if(L==old && RDS32(L+0x4c)==r && RDS32(L+0x50)==c) return 1;          // already there
    if(L!=old){ if(mem_ok(old)) *(volatile u8*)(old+8)&=(u8)~0x0c; *(volatile u8*)(L+8)|=0x0c;
      *(volatile u64*)(g+G_HAND_OWNER)=L; *(volatile u64*)(g+G_STCUR)=nd; }
    if(RDS32(L+0x58)>1) set_col(L,c);
    ((SetIdxFn)(g_main+0x5de5b0))(L,r); list_sound(L);
    tm_log2("status item",(s64)(L-g),r*10+c); return 1; }
  return 0; }
// Inventory popup over the status sheet (A Inventory): two side-by-side lists G+0x5d068 (Items / Trade / Store All) and
// G+0x5d568 (Abilities / Combat Arts / Battalions / Reclass), vtable 0x1a40890, rows 33.3 apart, columns 187.4 apart.
#define G_STPOP_L 0x5d068
#define G_STPOP_R 0x5d568
static int status_popup_tap(u64 g,u64 o,float x,float y){
  u64 L=g+G_STPOP_L, R=g+G_STPOP_R; if(o!=L && o!=R) return 1;
  float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f; s32 idx=RDS32(o+0x4c);
  float lx= o==L ? hx : hx-187.4f, row0=hy+4.f-33.3f*(float)idx;
  int col= (x>=lx-25.f&&x<lx+165.f) ? 0 : (x>=lx+165.f&&x<=lx+355.f) ? 1 : -1;
  float f=(y-row0)/33.3f; int r=(int)(f+(f>=0?0.5f:-0.5f));
  if(col<0 || r<0 || f>3.6f || f<-0.5f) return 1;              // outside the popup: swallow
  u64 t= col ? R : L; s32 cnt=RDS32(t+0x54); if(r>=cnt) return 1;
  if(t!=o){ // other column: move the focus instantly (list +8 flags 0x3c/0x30, hand owner G+0x58, column G+0x5da78)
    *(volatile u8*)(o+8)=0x30; *(volatile u8*)(t+8)=0x3c; *(volatile u64*)(g+G_HAND_OWNER)=t; *(volatile s32*)(g+0x5da78)=col;
    ((SetIdxFn)(g_main+0x5de5b0))(t,r); g_learn_hold=20; q_push(0,2,0); press(BTN_A); tm_log2("status popup col",col*10+r,idx); return 1; }
  list_jump(o,r,1); tm_log2("status popup",col*10+r,idx); return 1; }
// Details mode is up: the hand belongs to a list inside the sheet. The focus graph's current node (G+0x602c8, 0 outside Details)
// is set a little earlier, which is enough for status_instant (it sets the owner itself).
static int status_det_now(void){ u64 g=cmn_ui(); if(!g || !is_widget(g+G_STATUS,0x1a458f8)) return 0;
  u64 cur=RD64(g+0x602c8); if(cur>=g+0x60178 && cur<g+0x602c8) return 1;
  if(!hand_on(g)) return 0; u64 o=RD64(g+G_HAND_OWNER); return o>g+G_STATUS && o<g+G_STATUS_TAB; }
static int status_tap(u64 g,float x,float y){
  if(!is_widget(g+G_STATUS,0x1a458f8)) return 0;
  u64 o; u64 vt=hand_owner_vt(g,&o);
  // the sheet stays visible under the screens opened from its Inventory popup (Items, Abilities, Reclass, ...): leave those alone
  if(hand_on(g) && vt!=0x1a40890 && !(o>g+G_STATUS && o<g+G_STATUS_TAB)) return 0;
  if(hand_on(g) && vt==0x1a40890) return status_popup_tap(g,o,x,y);
  // Details mode (X): the hand walks over the item lists of the sheet (owners inside the sheet object). Taps on an item
  // walk the hand there with the game's own d-pad navigation, so the help popups follow (hand_chase).
  int det= (hand_on(g) && o>g+G_STATUS && o<g+G_STATUS_TAB) || (RD64(g+0x602c8)>=g+0x60178 && RD64(g+0x602c8)<g+0x602c8);
  if(det){
    if(inbox(x,y,1140.f,1264.f,682.f,718.f)){ press(BTN_X); return 1; }       // X Details (leave)
    if(y>=22.f&&y<=75.f){   // tabs: the arrows step (ZL / ZR work in Details mode); a tab name leaves Details first, then jumps there
      if((x>=600.f&&x<695.f)||(x>=1185.f&&x<=1275.f)) goto tabs;
      if(x>=696.8f && x<1185.f && rt_depth==0){ press(BTN_X); retap_later(x,y,12); }
      return 1; }
    if(inbox(x,y,0.f,65.f,315.f,410.f)){ press(BTN_LB); return 1; }
    if(inbox(x,y,1215.f,1280.f,315.f,410.f)){ press(BTN_RB); return 1; }
    if(inbox(x,y,60.f,1225.f,90.f,680.f)) status_instant(g,x,y);
    return 1; }
  // G+0x5da80 = how the sheet was opened: 0 (roster, Inventory, battle prep ...) shows X Details / A Inventory / B Back; other
  // values (4 = Goals) have no Inventory, and "X Details" sits where "A Inventory" would be
  if(RDS32(g+0x5da80)!=0){ static const Prompt p2[]={ {996.f,1120.f,BTN_X}, {1166.f,1264.f,BTN_B} }; if(prompt_tap(x,y,p2,NP(p2))) return 1; }
  else if(prompt_tap(x,y,status_prompts,NP(status_prompts))) return 1;
  tabs:
  if(y>=22.f&&y<=75.f){
    if(x>=600.f&&x<695.f){ press(BTN_ZL); return 1; }
    if(x>=1185.f&&x<=1275.f){ press(BTN_ZR); return 1; }
    s32 tab=RDS32(g+G_STATUS_TAB); if(tab<0||tab>4) return 1;
    int k=(int)((x-(744.f-47.2f))/94.4f); if(x<696.8f||k<0||k>4||k==tab) return 1;
    g_learn_hold=20; *(volatile s32*)(g+G_STATUS_TAB)=(k+4)%5; press(BTN_ZR); tm_log2("status tab",k,tab); return 1; }
  if(inbox(x,y,0.f,65.f,315.f,410.f)){ press(BTN_LB); return 1; }       // L: previous unit
  if(inbox(x,y,1215.f,1280.f,315.f,410.f)){ press(BTN_RB); return 1; }  // R: next unit
  // a tap on the sheet outside details mode: open Details (X), then put the hand on the tapped item as soon as Details is up
  if(!det && !hand_on(g) && rt_depth==0 && inbox(x,y,90.f,1215.f,100.f,675.f)){ press(BTN_X); retap_later(x,y,90); rt_wait_det=1; tm_log2("status details",(s64)x,(s64)y); }
  return 1; }

// ---- Quest details (book -> Quests -> A): G+0x9a358 (vt 0x1a21ca8); arrows above/below the right card switch quests ----
static int quest_detail_tap(u64 g,float x,float y){
  if(hand_on(g) || !is_widget(g+0x9a358,0x1a21ca8)) return 0;
  if(inbox(x,y,1166.f,1264.f,682.f,718.f)){ press(BTN_B); return 1; }
  if(inbox(x,y,825.f,925.f,20.f,95.f)){ press(BTN_U); return 1; }
  if(inbox(x,y,825.f,925.f,575.f,650.f)){ press(BTN_D); return 1; }
  return 1; }

// ---- Unit Appearance: options popup (vt 0x1a20eb0) and attire list (vt 0x1a21070): X Zoom In, - Swap display ----
static const Prompt appear_prompts[]={ {792.f,913.f,BTN_X}, {962.f,1108.f,BTN_MINUS} };
// "- Swap display" hides the menus and the hand (the list's state +0x38 becomes 5), the prompts and the turntable stay
static int appearance_screen(u64 g){ u64 o; u64 vt=hand_owner_vt(g,&o); if(vt!=0x1a20eb0 && vt!=0x1a21070) return 0;
  if(hand_on(g)) return 1;
  return RDS32(o+0x38)==5 && (*(volatile u8*)(o+8)&0x0c) && !is_widget(g+0x31120,0x1a44f48); }

// ---- Support: unit grid (vt 0x1a44138) ZL / ZR page arrows; support list (vt 0x1a43a68) L / R unit arrows ----
// ---- Reclass: class grid (vt 0x1a1ec48). The hand sits left of the selected class card; card centre = hand + (165, 22). ----
// Goals -> ZR Classes, category view (grid vt 0x1a1ec48, columns <= 1): category k has n_k classes (ints at +0x518), shown 6 per card
// row (100 px), the first row's top at y 122 when not scrolled; the scroll is the grid's +0x514 (1080p, -150 per card row scrolled
// down). Returns the category under y, -1 for none, -2 when the counts look wrong.
static int goal_cat_at(u64 o,float y){ s32 cnt=RDS32(o+0x54); if(cnt<=0||cnt>8) return -2;
  int r0[8], rows[8], tot=0;
  for(int k=0;k<cnt;k++){ s32 n=RDS32(o+0x518+(u64)k*4); if(n<0||n>40) return -2; rows[k]= n<=6 ? 1 : (n+5)/6; r0[k]=tot; tot+=rows[k]; }
  if(y<110.f||y>625.f) return -1;
  float sf=-RDF(o+0x514)/150.f; int sc=(int)(sf+(sf>=0?0.5f:-0.5f));
  int vr=(int)((y-122.f)/100.f+100.f)-100+sc; for(int k=0;k<cnt;k++) if(vr>=r0[k] && vr<r0[k]+rows[k]) return k;
  return -1; }
static int header_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0; u64 o; u64 vt=hand_owner_vt(g,&o);
  // right after a popup closes (shop Buy / Cancel) the game leaves G+0x58 empty while the hand is back on the list: use the list
  if(!o){ o=own_fallback(); vt= o ? RD64(o)-g_main : 0; }
  if(vt==0x1a1b0b0 &&inbox(x,y,1110.f,1264.f,682.f,718.f)){ press(BTN_X); return 1; }   // dining hall Today's Special: X Support Info
  if(vt==0x1a44a48 && inbox(x,y,1145.f,1264.f,682.f,718.f)){ press(BTN_X); return 1; }   // Battlefield (mission list + map, from the calendar): X Menu
  if(vt==0x1a1e048 && y>=682.f&&y<=718.f){ if(x>=790.f&&x<955.f){ press(BTN_PLUS); return 1; } if(x>=995.f&&x<1110.f){ press(BTN_X); return 1; } if(x>=1145.f&&x<=1264.f){ press(BTN_ZR); return 1; } }   // Instruct: + Auto-Instruct / X Status / ZR Classes
  if(vt==0x1a1ddc8 && y>=682.f&&y<=718.f){ if(x>=995.f&&x<1110.f){ press(BTN_X); return 1; } if(x>=1145.f&&x<=1264.f){ press(BTN_ZR); return 1; } }   // Instruct topics: X Status / ZR Classes
  if((vt==0x1a47f78||vt==0x1a462e8) && y>=88.f&&y<=128.f){ if(x>=830.f&&x<=905.f){ press(BTN_LB); return 1; } if(x>=1165.f&&x<=1245.f){ press(BTN_RB); return 1; } }   // shops, blacksmith Forge / Repair List: L / R unit arrows
  if(vt==0x1a44138 && y>=88.f&&y<=128.f){ if(x>=35.f&&x<=115.f) press(BTN_ZL); else if(x>=725.f&&x<=805.f) press(BTN_ZR); return 1; }
  if(vt==0x1a43a68 && y>=88.f&&y<=128.f){ if(x>=820.f&&x<=910.f) press(BTN_LB); else if(x>=1170.f&&x<=1260.f) press(BTN_RB); return 1; }
  if((vt==0x1a24f40||vt==0x1a250f8) && y>=110.f&&y<=152.f){                     // Goals (unit chosen): L / R unit arrows
    if(x>=815.f&&x<=895.f){ press(BTN_LB); return 1; } if(x>=1165.f&&x<=1250.f){ press(BTN_RB); return 1; } }
  if((vt==0x1a24d78||vt==0x1a24f40||vt==0x1a250f8) && y>=682.f&&y<=718.f){   // Goals: X Status / ZR Classes
    if(x>=1005.f&&x<1110.f){ press(BTN_X); return 1; } if(x>=1140.f&&x<=1264.f){ press(BTN_ZR); return 1; } }
  // Goals -> ZR Classes / Reclass: the hand is on the unit list (vt 0x1a1ea90) and the class grid (unit list + 0x620, vt 0x1a1ec48) is
  // already shown on the right for that unit: a tap there enters it (A) and is replayed the moment the grid takes the hand, which
  // selects the tapped category / class (a second tap confirms)
  // Category view (grid columns <= 1): instant. The grid keeps its index when it takes the hand, so the tapped category is set first
  // (SetIndex) and A enters the grid right on it (help text and highlight follow); the next tap on it confirms.
  if(vt==0x1a1ea90 && inbox(x,y,480.f,1245.f,110.f,625.f)){ u64 gw=o+0x620; if(!is_widget(gw,0x1a1ec48)) return 1;
    if(RDS32(gw+0x58)<=1){ int t= x>=470.f && x<=1235.f ? goal_cat_at(gw,y) : -1; if(t<0) return 1;
      if(RDS32(gw+0x4c)!=t){ g_learn_hold=20; ((SetIdxFn)(g_main+0x5de5b0))(gw,t); }
      press(BTN_A); dt_arm(gw+0x4c,t,0,0); dt_own=gw; tm_log2("classes from unit instant",t,0); return 1; }   // armed for the grid: the next tap on it confirms
    if(rt_depth==0){ press(BTN_A); retap_on_change(x,y,30); rt_settle=1; tm_log2("classes from unit",(s64)x,(s64)y); } return 1; }
  if((vt==0x1a44878||vt==0x1a1ea90||vt==0x1a1ec48||vt==0x1a1ee00) && inbox(x,y,1166.f,1264.f,682.f,718.f)){ press(BTN_X); return 1; } // X Status
  // Reclass: class list (vt 0x1a1ee00) with the class info panel on the right; ZL / ZR switch the panel's pages
  if(vt==0x1a1ee00 && y>=150.f&&y<=192.f){ if(x>=860.f&&x<=935.f){ press(BTN_ZL); return 1; } if(x>=1175.f&&x<=1250.f){ press(BTN_ZR); return 1; } }
  if(vt==0x1a1ec48 && RDS32(o+0x58)<=1){   // Goals -> ZR Classes: only the category headers take the hand (1 or 2 card rows tall)
    s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); if(cnt<=0||cnt>20||idx<0||idx>=cnt) return 1;
    if(x<470.f){ if(x>=30.f && y>=110.f && y<=620.f && rt_depth==0){ press(BTN_B); retap_on_change(x,y,15); } return 1; }   // unit list: back there, the tap again as soon as it has the hand
    if(x>1235.f||y<110.f||y>625.f) return 1;
    float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f, dy=y-(hy+20.f); if(dy<0) dy=-dy;   // the hand sits ~20 px above the header's centre
    // Layout: category k has n_k classes (ints at +0x518), shown 6 per card row (100 px), the first row's top at y 122 when not
    // scrolled; the hand sits 28 px above the middle of its header. The scroll is the grid's +0x514 (1080p, -150 per card row), then the
    // tapped card row gives the category.
    int t=goal_cat_at(o,y);
    if(t!=-2){
      if(t<0) return 1;
      if(t==idx){ dt_on=0; press(BTN_A); return 1; }   // the selected category: one tap
      list_jump(o,t,0); dt_arm(o+0x4c,t,0,0); tm_log2("goal class row",t,idx); return 1; }
    if(dy<=40.f){ dt_on=0; press(BTN_A); return 1; }
    hand_chase(hx+10.f,y-20.f); hc_tol=40.f; hc_arm_w=o; tm_log2("goal class chase",(s64)y,idx); return 1; }
  if(vt==0x1a1ec48){
    s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54), cidx=RDS32(o+0x50), ccnt=RDS32(o+0x58); if(cnt<=0||cnt>20||idx<0||idx>=cnt) return 1;
    if(ccnt<1) ccnt=1; if(cidx<0||cidx>=ccnt) cidx=0;
    float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f, xp=learned_xpitch((u32)vt), yp=learned_pitch((u32)vt);
    float fx=(x-(hx+165.f))/xp, fy=(y-(hy+22.f))/yp;
    int dc=(int)(fx+(fx>=0?0.5f:-0.5f)), dr=(int)(fy+(fy>=0?0.5f:-0.5f));
    float ex=fx-(float)dc, ey=fy-(float)dr; if(ex<0)ex=-ex; if(ey<0)ey=-ey;
    if(ex>0.47f||ey>0.47f) return 1;                    // between cards
    int r=idx+dr, c=cidx+dc; if(r<0||r>=cnt||c<0||c>=ccnt) return 1;
    grid_jump(o,r,c,rt_depth==0); tm_log2("reclass",r*100+c,idx*100+cidx); return 1; }   // replayed from the unit list: select only
  return 0; }

// ---- Battle preparation (battle phase 28 wheel, 29 Units, 30 Map) ----
// Wheel: list-derived controller at HUD+0x9c720 (vt 0x19f4490). Selected wedge +0x8c (0 Units, 1 Inventory, 2 Save,
// 3 System, 4 Quit, 5 Marketplace, 6 Support, 7 Map, 8 Fight! in the centre), state +0x88 (2 idle; the game sets 1 when
// the stick changes the wedge, which then animates and returns to 2), enabled flags bytes +0x94+i. Centre (640,320).
#define PREP_WHEEL 0x9c720
#define VT_PREP_WHEEL 0x19f4490
static u64 prep_wheel(void){ u64 h=hud(); if(!h) return 0; u64 o=h+PREP_WHEEL; if(!mem_ok(o)||RD64(o)!=g_main+VT_PREP_WHEEL||!widget_vis(o)) return 0; return o; }
static s32 prep_phase(void){ u64 b=battle(); if(!b||!hud()) return -1; return RDS32(b+0x3cc0); }
static int pw_n, pw_t;   // wait for the wheel to settle on wedge pw_t, then press A
static void prep_tick(void){
#ifdef TM_DEBUG
  { static s64 last=-1; u64 h=hud(), g=cmn_ui(); s32 ph=prep_phase(); if(h && g && ph>=28 && ph<=33){ u64 o=h+PREP_WHEEL, v=RD64(o+0x80);
      s64 sig=(s64)ph*10000000+hand_on(g)*1000000+widget_vis(o)*100000+RDS32(o+0x88)*10000+RDS32(o+0x8c)*1000+(mem_ok(v)?RDS32(v+0x98)*10+(RD64(v+0xa8)!=0):0);
      if(sig!=last){ last=sig; tm_log2("pw trace",(s64)g_poll,sig); } } }
  if(g_ht_n>0){ static s64 hl=-1; u64 g=cmn_ui(); g_ht_n--; if(g){   // hand trace: x, y (1080p) and the owner's offset in the HUD / G
      u64 o=RD64(g+0x58), h=hud(); s64 s=(s64)RDF(g+0x2c)*10000+(s64)RDF(g+0x30); if(s!=hl || g_ht_n==29){ hl=s;
        tm_log2("hand trace",(s64)g_poll*1000000+(s64)(h&&o>h&&o<h+0x100000?o-h:o-g),s); } } }
#endif
  if(pw_n<=0) return; pw_n--; if(q_busy()) return; u64 o=prep_wheel(); if(!o){ pw_n=0; return; }
  if(RDS32(o+0x88)==2 && RDS32(o+0x8c)==pw_t){ press(BTN_A); pw_n=0; } }
static const Prompt prep_wheel_prompts[]={ {955.f,1095.f,BTN_X}, {1135.f,1264.f,BTN_A} };
static const Prompt prep_units_prompts[]={ {765.f,940.f,BTN_ZL}, {985.f,1090.f,BTN_X}, {1135.f,1264.f,BTN_RB} };
// With adjutants unlocked, a deployed unit other than Byleth also gets "Y Set Adjutant" (Units list and R Unit Info), which shifts the
// prompts on its left. The prompt bar's
// entries are u32 codes at G+0xf0 (count G+0x118, right to left: 0x800 R, 0x8000 Y, 0x1000 X, 0x100 ZL)
static const Prompt prep_units_adj_prompts[]={ {576.f,745.f,BTN_ZL}, {795.f,895.f,BTN_X}, {947.f,1092.f,BTN_Y}, {1133.f,1264.f,BTN_RB} };
static int prompt_bar_has(u64 g,u32 code){ s32 n=RDS32(g+0x118); if(n<0||n>10) return 0; for(int i=0;i<n;i++) if(RD32(g+0xf0+4*i)==code) return 1; return 0; }
static const Prompt prep_info_prompts[]={ {1010.f,1120.f,BTN_X}, {1160.f,1264.f,BTN_RB} };
static const Prompt prep_info_adj_prompts[]={ {824.f,925.f,BTN_X}, {970.f,1118.f,BTN_Y}, {1155.f,1264.f,BTN_RB} };
static int prep_tap(u64 g,float x,float y){
  s32 ph=prep_phase(); if(ph<28||ph>30) return 0;
  if(ph==29){ u64 vt=hand_owner_vt(g,0); if(!hand_on(g)) return 0;
    if(vt==0x1a44878){ if(prompt_bar_has(g,0x8000)){ if(prompt_tap(x,y,prep_units_adj_prompts,NP(prep_units_adj_prompts))) return 1; }
      else if(prompt_tap(x,y,prep_units_prompts,NP(prep_units_prompts))) return 1; }
    if(vt==0x1a436f8){   // R Unit Info table: X Status / (Y Set Adjutant) / R Back
      if(prompt_bar_has(g,0x8000)){ if(prompt_tap(x,y,prep_info_adj_prompts,NP(prep_info_adj_prompts))) return 1; }
      else if(prompt_tap(x,y,prep_info_prompts,NP(prep_info_prompts))) return 1; }
    return 0; }
  if(ph!=28 || hand_on(g)) return 0;
  u64 o=prep_wheel(); if(!o) return 0;
  if(prompt_tap(x,y,prep_wheel_prompts,NP(prep_wheel_prompts))) return 1;
  float dx=x-640.f, dy=y-320.f, d2=dx*dx+dy*dy; if(d2>300.f*300.f) return 1;
  int t=8;
  if(d2>118.f*118.f){ static const float sx[8]={0.f,0.7071f,1.f,0.7071f,0.f,-0.7071f,-1.f,-0.7071f}, sy[8]={-1.f,-0.7071f,0.f,0.7071f,1.f,0.7071f,0.f,-0.7071f};
    float bd=-1e9f; for(int k=0;k<8;k++){ float d=dx*sx[k]+dy*sy[k]; if(d>bd){ bd=d; t=k; } } }
  s32 cur=RDS32(o+0x8c), st=RDS32(o+0x88); if(cur<0||cur>8) return 1;
  if(!*(volatile u8*)(o+0x94+t)) return 1;            // disabled wedge
  if(t==cur){ if(st==2) press(BTN_A); return 1; }
  if(st!=2) return 1;
  *(volatile s32*)(o+0x8c)=t; *(volatile s32*)(o+0x88)=1; pw_t=t; pw_n=90;
  tm_log2("prep wheel",t,cur); return 1; }

// ---- Advice box (cathedral Counselor: "Would you like to read a note from the advice box?" -> Yes): the note with three timed
// answers is a list inside the monastery UI object M = *(main+0x1afb448) at M+0x301d0 (vt 0x1a1bca8; idx +0x4c, cnt +0x54; byte +0x21 =
// shown, 0 once answered). The common hand owner G+0x58 is a hidden widget meanwhile, so the generic lists never got the taps. Answer
// rows (720p): middles 456.3 / 519 / 581.6, x 339-945. One tap answers (user: all answers are instant clicks).
static int advice_tap(u64 g,float x,float y){
  (void)g; u64 m=RD64(g_main+0x1afb448); if(!mem_ok(m)) return 0; u64 w=m+0x301d0; if(!is_widget(w,0x1a1bca8)) return 0;
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54); if(cnt<1||cnt>3||idx<0||idx>=cnt) return 1;
  if(x<339.f||x>945.f) return 1;
  float f=(y-456.3f)/62.7f; if(f<-0.5f) return 1; int r=(int)(f+0.5f); if(r>=cnt) return 1;
  if(r==idx && !q_busy()) press(BTN_A); else list_jump(w,r,1);
  tm_log2("advice",r,idx); return 1; }
// ---- Library books (free roam): the reader is a list widget inside the monastery UI object M = *(main+0x1afb448),
// at M+0x8fdd0 (vt 0x1a22530; page +0x50, pages +0x54). Page arrows under the text, "B Close" prompt; other taps do nothing.
// (The free-roam HUD stays visible underneath, so without this the corner prompts would get the taps.)
static int book_tap(u64 g,float x,float y){
  u64 m=RD64(g_main+0x1afb448); if(!mem_ok(m)) return 0; u64 bk=m+0x8fdd0; if(!is_widget(bk,0x1a22530)) return 0;
  if(inbox(x,y,1155.f,1264.f,682.f,718.f)){ press(BTN_B); return 1; }
  if(y>=490.f&&y<=550.f){ if(x>=545.f&&x<=620.f){ press(BTN_L); return 1; } if(x>=660.f&&x<=735.f){ press(BTN_R); return 1; } }
  return 1; }

// ---- Calendar animation after "Finish Exploring" (G+0x9f4c0, vt 0x1a3c890): "+ Skip" prompt; other taps fall through (A) ----
// ---- Lecture results (G+0x36e8, vt 0x1a3a600): the students scroll by with "+ Skip" in the corner; once the list has the focus
// (+8 bits 0x0c) the prompt reads "A Close" ----
static int results_tap(u64 g,float x,float y){
  u64 w=g+0x36e8; if(!is_widget(w,0x1a3a600) || is_widget(g+0x9f4c0,0x1a3c890)) return 0;   // the week animation in front: its "+ Skip" (calanim_tap)
  if(inbox(x,y,1150.f,1264.f,682.f,718.f)){ int done=(*(volatile u8*)(w+8)&0x0c)==0x0c; press(done?BTN_A:BTN_PLUS); tm_log2("results prompt",done,0); return 1; }
  return 0; }
static int calanim_tap(u64 g,float x,float y){
  if(!is_widget(g+0x9f4c0,0x1a3c890)) return 0;
  if(hand_on(g) && !inbox(x,y,1155.f,1264.f,682.f,718.f)) return 0;
  if(inbox(x,y,1155.f,1264.f,682.f,718.f)){ press(BTN_PLUS); tm_log2("calendar skip",(s64)x,(s64)y); return 1; }
  return 0; }

// ---- House choice (audience chamber): a 1x3 picker (vt 0x1a410a8, column +0x50 = house, +0x58 = 3) referenced from
// main+0x21d9b88. No hand. Arrows / side banners = Left / Right, B Back, A Confirm; other taps do nothing.
static int house_tap(u64 g,float x,float y){
  u64 hp=RD64(g_main+0x21d9b88); if(!is_widget(hp,0x1a410a8) || hand_on(g) || RDS32(hp+0x58)!=3) return 0;
  if(!is_widget(g+0xbd7bc,0x1a2d8a8) || !(*(volatile u8*)(hp+8)&0x0c)) return 0;   // only inside the event scene, picker focused
  if(y>=682.f&&y<=718.f){ if(x>=990.f&&x<=1100.f){ press(BTN_B); return 1; } if(x>=1135.f&&x<=1264.f){ press(BTN_A); return 1; } }
  if(y>=140.f&&y<=440.f){ if(x>=40.f&&x<=295.f){ press(BTN_L); tm_log2("house",-1,0); return 1; }
    if(x>=995.f&&x<=1250.f){ press(BTN_R); tm_log2("house",1,0); return 1; } }
  if(inbox(x,y,395.f,530.f,305.f,585.f)){ press(BTN_A); return 1; }   // the chosen house's banner: confirm (a Yes/No follows)
  return 1; }

// ---- Extras: Movie Gallery (list vt 0x1a4e660; "X Change Settings" moves the hand to the avatar picker vt 0x1a4eb60,
// Left/Right pick the male/female avatar, B back) and Music Library (list vt 0x1a4ddf0; G+0xa5e50 != 0 while a song plays,
// then the prompt is "B Stop" and songs with variants show "X Rain" / "X Thunder" under the title) ----
static int extras_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0; u64 o; u64 vt=hand_owner_vt(g,&o);
  if(vt==0x1a4e660 || vt==0x1a4eb60){
    if(inbox(x,y,1075.f,1260.f,503.f,542.f)){ press(BTN_Y); return 1; }                       // Y Turn Subtitles On/Off
    if(inbox(x,y,850.f,1230.f,180.f,240.f)){                                                   // avatar picker: arrows and portraits
      if(vt==0x1a4e660) press(BTN_X);
      press(x<1035.f?BTN_L:BTN_R); tm_log2("movie avatar",(s64)x,(s64)vt); return 1; }
    if(vt==0x1a4e660 && inbox(x,y,1100.f,1264.f,682.f,718.f)){ press(BTN_X); return 1; }      // X Change Settings
    // back to the list, instant: the list (avatar picker - 0x3b80) gets the tapped row with SetIndex while it hasn't the focus, then
    // B moves the hand straight onto it (rows 34.67 apart, row 0 centre y 160.3, 11 visible from +0x6a); selected, a second tap plays
    if(vt==0x1a4eb60){ if(x<600.f && y>140.f && y<570.f){ u64 l=o-0x3b80; if(!is_widget(l,0x1a4e660)){ press(BTN_B); retap_later(x,y,15); return 1; }
        int r=(int)((y-160.3f)/34.67f+0.5f), top=*(volatile short*)(l+0x6a), n=RDS32(l+0x54), it=top+r; if(r<0||r>=*(volatile short*)(l+0x66)||it<0||it>=n) return 1;
        if(RDS32(l+0x4c)!=it) ((SetIdxFn)(g_main+0x5de5b0))(l,it);
        press(BTN_B); dt_arm(l+0x4c,it,0,0); tm_log2("movie list from settings",it,0); }
      return 1; }
    return 0; }
  if(vt==0x1a4ddf0){
    int playing=RDS32(g+0xa5e50)!=0;
    if(inbox(x,y,1100.f,1264.f,682.f,718.f)){ press(playing?BTN_B:BTN_A); return 1; }        // A Play / B Stop
    if(playing && inbox(x,y,900.f,1250.f,338.f,385.f)){ press(BTN_X); tm_log2("music variant",(s64)x,0); return 1; } // X Rain / Thunder
    return 0; }
  return 0; }
// ---- DLC Supplies (Byleth's room, by the bed): set list vt 0x1a25620 on the left, "X Details" moves the hand into the
// Set Contents list vt 0x1a257e0 on the right (rows 35 px apart from the hand, only shows each item's text). Prompts X Details / A Confirm. ----
static const Prompt dlc_prompts[]={ {975.f,1100.f,BTN_X}, {1135.f,1264.f,BTN_A} };
// Focus: list +8 = 0x3c (focused) / 0x30. After X or B leaves Details, G+0x58 still points at the contents list (the hand
// is parked top left), so the mode is read from the focus bits. The set list sits 0x618 before the contents list.
static int dlc_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0; u64 o; u64 vt=hand_owner_vt(g,&o);
  u64 sl=0, cl=0;
  if(vt==0x1a25620){ sl=o; cl=o+0x618; } else if(vt==0x1a257e0){ cl=o; sl=o-0x618; } else return 0;
  if(!is_widget(sl,0x1a25620) || !is_widget(cl,0x1a257e0)) return 0;
  if(prompt_tap(x,y,dlc_prompts,NP(dlc_prompts))) return 1;
  int det= (*(volatile u8*)(cl+8)&0x0c)==0x0c;
  int right= x>=555.f && x<=1225.f && y>=160.f && y<=400.f;
  if(!det){
    if(right){ if(rt_depth==0){ press(BTN_X); retap_later(x,y,15); } return 1; }   // contents: open Details first
    s32 idx=RDS32(sl+0x4c), cnt=RDS32(sl+0x54); if(cnt<=0||cnt>20||idx<0||idx>=cnt) return 1;
    if(x<105.f||x>475.f) return 1;
    float f=(y-152.f)/35.f; int r=(int)(f+(f>=0?0.5f:-0.5f)); if(r<0||r>=cnt) return 1;
    list_pick(sl,r); tm_log2("dlc set",r,idx); return 1; }
  if(!right){ if(x<480.f && y<230.f && rt_depth==0){ press(BTN_B); retap_later(x,y,15); } return 1; }   // set list: leave Details
  s32 idx=RDS32(cl+0x4c), cnt=RDS32(cl+0x54); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 1;
  float hy=RDF(g+0x30)/1.5f, f=(y-(hy+3.f))/35.f; int d=(int)(f+(f>=0?0.5f:-0.5f)); if(idx+d<0||idx+d>=cnt) return 1;
  if(d) list_jump(cl,idx+d,0); tm_log2("dlc contents",idx+d,idx); return 1; }
// ---- Storehouse / shop "Sell how many?" dialog: quantity picker on top (no hand), Sell / Cancel buttons below = a 1x2 grid at G+0x8d8b0
// (vt 0x1a241a8, column +0x50, focused when +8 has 0x0c). Buttons: Sell x 385-635, Cancel 645-890, y 425-470; the quantity arrows at
// x ~728 / ~857, y ~302. ----
static int sell_dialog_tap(u64 g,float x,float y){
  // the same dialog in the shops (Sell -> an item stack from the convoy): G+0x8b0f8 over the shop list G+0x85418
  u64 w=g+0x8d8b0;
  if(!(is_widget(w,0x1a241a8) && RDS32(w+0x58)==2 && is_widget(g+0x8eb38,0x1a48aa8))){   // over the storehouse list
    w=g+0x8b0f8; if(!(is_widget(w,0x1a241a8) && RDS32(w+0x58)==2 && is_widget(g+0x85418,0x1a47f78))) return 0; }
  int foc=(*(volatile u8*)(w+8)&0x0c)==0x0c && hand_on(g) && RD64(g+G_HAND_OWNER)==w;
  if(inbox(x,y,1150.f,1264.f,682.f,718.f)){ press(BTN_A); return 1; }   // "A Confirm" in the corner
  if(y>=420.f&&y<=475.f&&x>=385.f&&x<=890.f){ int c= x<640.f ? 0 : 1;
    s32 cur=RDS32(w+0x50);
    // focus on the quantity: A there moves the focus to Sell (a second A right away sells), B closes the dialog (= Cancel)
    if(!foc){ if(c==0){ q_push(BTN_A,2,3); q_push(BTN_A,2,3); } else press(BTN_B); tm_log2("sell dialog instant",c,cur); return 1; }
    if(c!=cur) set_col(w,c); press(BTN_A); tm_log2("sell dialog",c,cur); return 1; }
  if(y>=275.f&&y<=330.f&&x>=690.f&&x<=900.f){ int r= x>=790.f;
    if(foc) q_push(BTN_U,2,2);                       // buttons have the focus: up to the quantity, then the arrow in the same go
    press(r?BTN_R:BTN_L); return 1; }
  return 1; }
// ---- Battalion Guild (marketplace; menu Hire / Dismiss / Replenish = vt 0x1a083f0, one tap) ----
// Hire: unit list G+0x73788 (vt 0x1a46d08, rows 53.3), then "Available for Hire" G+0x73e48 (vt 0x1a47098, rows 34.67, the hand ~3 px
// above a row's middle) with L / R unit arrows on the unit panel. Dismiss reuses that list (help bar "Select a battalion to dismiss.")
// with "X Sort by" above it. Replenish: G+0x7cb70 (vt 0x1a474e8, rows 34.7) with ZL / ZR pages, a "+ Replenish" button and the prompts
// X Details / + Confirm / - Select/Deselect All; A on a row ticks / unticks it, so a tap on a row does that directly.
static const Prompt guild_rep_prompts[]={ {765.f,850.f,BTN_X}, {905.f,1030.f,BTN_PLUS}, {1075.f,1264.f,BTN_MINUS} };
static int guild_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0; u64 o; u64 vt=hand_owner_vt(g,&o);
  if(vt!=0x1a474e8 && vt!=0x1a47098) return 0;
  if(!widget_vis(o)) return 0;
  s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); float hy=RDF(g+0x30)/1.5f;
  if(vt==0x1a474e8){
    if(prompt_tap(x,y,guild_rep_prompts,NP(guild_rep_prompts))) return 1;
    if(inbox(x,y,945.f,1220.f,228.f,280.f)){ press(BTN_PLUS); tm_log2("guild replenish",0,0); return 1; }
    if(y>=88.f&&y<=126.f){ if(x>=35.f&&x<=115.f) press(BTN_ZL); else if(x>=750.f&&x<=830.f) press(BTN_ZR); return 1; }
    if(cnt<=0||cnt>60||idx<0||idx>=cnt||x<50.f||x>800.f) return 1;
    float f=(y-(hy+2.7f))/34.7f; int d=(int)(f+(f>=0?0.5f:-0.5f)); int t=idx+d; if(t<0||t>=cnt) return 1;
    list_jump(o,t,1); tm_log2("guild tick",t,idx); return 1; }
  int dismiss=help_text_is(g,"Select a battalion to dismiss");
  if(dismiss && inbox(x,y,505.f,755.f,42.f,84.f)){ press(BTN_X); return 1; }        // X Sort by
  if(!dismiss && y>=88.f && y<=128.f){ if(x>=830.f&&x<=900.f){ press(BTN_LB); return 1; } if(x>=1185.f&&x<=1255.f){ press(BTN_RB); return 1; } }
  if(cnt<=0||cnt>60||idx<0||idx>=cnt) return 1;
  if(x<55.f||x>720.f||y<160.f||y>375.f) return 1;
  float f=(y-(hy+3.f))/34.67f; int d=(int)(f+(f>=0?0.5f:-0.5f)); int t=idx+d; if(t<0||t>=cnt) return 1;
  list_pick(o,t); tm_log2(dismiss?"guild dismiss":"guild hire",t,idx); return 1; }
// A lone "B Back" / "B Close" in the corner with no hand on screen (e.g. the amiibo Gazebo's "Please hold the amiibo to the NFC area."):
// the prompt bar has exactly one entry, B (0x4000); a tap on it presses B
static int lone_back_tap(u64 g,float x,float y){
  if(hand_on(g) || RDS32(g+0x118)!=1 || RD32(g+0xf0)!=0x4000 || !inbox(x,y,1150.f,1264.f,682.f,718.f)) return 0;
  press(BTN_B); tm_log2("lone back",0,0); return 1; }
static int screens_tap(u64 g,float x,float y){
  if(sell_dialog_tap(g,x,y)) return 1;
  if(extras_tap(g,x,y) || dlc_tap(g,x,y)) return 1;
  if(lone_back_tap(g,x,y) || prep_tap(g,x,y) || book_tap(g,x,y) || advice_tap(g,x,y) || results_tap(g,x,y) || calanim_tap(g,x,y) || house_tap(g,x,y)) return 1;
  if(guild_tap(g,x,y)) return 1;
  if(bdetail_tap(g,x,y) || calendar_tap(g,x,y) || roster_tap(g,x,y) || status_tap(g,x,y) || quest_detail_tap(g,x,y) || header_tap(g,x,y)) return 1;
  if(appearance_screen(g)){ if(prompt_tap(x,y,appear_prompts,NP(appear_prompts))) return 1; if(!hand_on(g)) return 1; }  // swap display: other taps do nothing
  return 0; }

// ---- Conversations: the message window G+0xf1eb4 (vt 0x1a2e688) is up. ZL Log / X Auto-Advance prompts; any other
// tap advances (A), also in battle talk scenes where the phase-based battle logic would otherwise get the tap.
// The Log screen (hand owner vt 0x1a3c630) closes with B from the same prompt.
#define G_TALKWIN 0xf1eb4
// cutscenes (event scenes) use another message window, G+0xbbc64 (vt 0x1a2edb0), with the same ZL Log / X Auto-Advance prompts
#define G_EVTWIN 0xbbc64
// the event message window class (vt 0x1a2edb0) is instanced at several places: cutscenes +0xbbc64, mealtime +0xf243c,
// battle quotes (a unit falls, boss lines) +0x1a28
static int talk_open(u64 g){ return is_widget(g+G_TALKWIN,0x1a2e688) || is_widget(g+G_EVTWIN,0x1a2edb0) || is_widget(g+0xf243c,0x1a2edb0) || is_widget(g+0x1a28,0x1a2edb0); }
static u64 log_own; static int log_arm;   // dialogue Log: tap-selected entry (the next tap on it replays the voice)
static const Prompt talk_prompts[]={ {938.f,1025.f,BTN_ZL}, {1079.f,1264.f,BTN_X} };
// Unit preview (first monastery day: a student's stats sheet with "X Swap display  A Ask again  B Back", between the lines in which
// Edelgard / Dimitri / Claude describes them): no hand and no talk window shown (all talk windows vis 0), so no handler took the taps.
// Known by the prompt bar: exactly B (0x4000), A (0x2000), X (0x1000), and the label of the middle one (pointers at G+0xa0 / +0xa8 /
// +0xb0) reading "Ask again". The bar alone isn't enough: it still read B / A / X at the start of the next line, with the talk window
// up (taps there must go on advancing the text). The prompts are tappable; other taps do nothing.
static const Prompt preview_prompts[]={ {800.f,945.f,BTN_X}, {993.f,1116.f,BTN_A}, {1169.f,1264.f,BTN_B} };
static int preview_tap(u64 g,float x,float y){
  if(hand_on(g) || talk_open(g) || RDS32(g+0x118)!=3 || RD32(g+0xf0)!=0x4000 || RD32(g+0xf4)!=0x2000 || RD32(g+0xf8)!=0x1000) return 0;
  u64 l=RD64(g+0xa8); if(!mem_ok(l)) return 0; const char*t="Ask again"; for(int k=0;k<10;k++) if(*(volatile char*)(l+k)!=t[k]) return 0;
  prompt_tap(x,y,preview_prompts,NP(preview_prompts)); return 1; }
static int talk_tap(u64 g,float x,float y){
  if(hand_on(g)){ u64 vt=hand_owner_vt(g,0);
    if(vt==0x1a3c630){ if(inbox(x,y,900.f,1264.f,682.f,718.f)){ press(BTN_B); return 1; }   // Log: the prompt row closes it
      // entries are cards 153.33 px apart (230 in 1080p, every card the same height), 4 on screen; the hand sits at the left of the
      // selected card, ~12 px above its middle. The Log keeps its own selection: owner+0x199f0 = selected entry counted from the
      // newest (0 = bottom), +0x199ec = entries, +0x199e0 = scroll (1080p px). A tap on another card is instant: the index goes
      // next to it and one real U / D step lands there (the game moves the hand and the highlight). A tap on the selected card
      // arms it, the next tap plays the voice line (A).
      u64 o=RD64(g+G_HAND_OWNER); if(o!=log_own){ log_own=o; log_arm=0; }
      if(x<330.f||x>960.f||y<40.f||y>680.f) return 1;
      float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f, f=(y-(hy+12.f))/153.33f; int d=(int)(f+(f>=0?0.5f:-0.5f));
      if(!d){ log_arm=0; press(BTN_A); tm_log2("log line",(s64)y,0); return 1; }   // the selected card: one tap plays it
      s32 n=RDS32(o+0x199ec), i=RDS32(o+0x199f0), t=i-d;
      if(n>0 && n<=5000 && i>=0 && i<n){ if(t<0||t>=n) return 1;
        s32 pre= d>0 ? t+1 : t-1; if(pre!=i) *(volatile s32*)(o+0x199f0)=pre; q_push(d>0?BTN_D:BTN_U,2,3); log_arm=1; tm_log2("log jump",t,i); return 1; }
      hand_chase(hx+10.f,y-12.f); hc_tol=70.f; log_arm=1; tm_log2("log walk",(s64)y,(s64)hy); return 1; }   // unknown layout: walk
    if(vt==0x1a48738 && inbox(x,y,1150.f,1264.f,682.f,718.f)){ press(BTN_ZL); return 1; } // pause menu: ZL Log
    // free days: the book keeps "L Finish Exploring" bottom left (the prompt bar G+0x30fa8, vt 0x1a44db8, stays up; L works from the book)
    if(vt==0x1a48738 && inbox(x,y,28.f,260.f,620.f,665.f) && is_widget(g+0x30fa8,0x1a44db8)){ press(BTN_LB); tm_log2("finish exploring (book)",(s64)x,0); return 1; }
    if(talk_open(g) && prompt_tap(x,y,talk_prompts,NP(talk_prompts))) return 1;           // dialogue choices: prompts still work
    return 0; }
  if(is_widget(g+0x31278,0x1a3c630) && RDS32(g+0x31278+0x54)<=0){ press(BTN_B); return 1; } // empty Log: any tap closes it
  if(!talk_open(g)) return 0;
  if(prompt_tap(x,y,talk_prompts,NP(talk_prompts))) return 1;
  press(BTN_A); tm_log2("talk",(s64)x,(s64)y); return 1; }

// ---- Battle unit details (X on a unit, battle phase 16). Six item lists inside the battle HUD, the same focus scheme as the status
// sheet: a navigation graph object at HUD+0x9ab60 (vt 0x19ee750): +0x10 current node, +0x30 byte = Details mode (X), +0x34 node
// count, nodes of 0x30 bytes from +0x38 {list, up, down, left, right, flags}. The focused list has +8 bits 0x0c, G+0x58 = list.
// Outside Details mode the hand sits on the right-hand list (Inventory / Combat Arts / Battalion page; page kind owner+0xfe0) and
// picks rows there. Lists (HUD offsets): 0x7c7c8 top panel (house, battalion icon / Lv, Exp / HP, Mv / class, movement type and, with
// a battalion's bonus, the Formation State icon at the portrait's corner),
// 0x80d60 combat stats, 0x7e350 stats (4x2), 0x81948 abilities (one row of 9 columns, row index stays -1), 0x7ff70 skill levels
// (moves with the page: its +0x30 = y origin, 1080p), 0x830b8 the right-hand list. Hit boxes are relative to each list's y origin
// (+0x30 / 1.5), so they follow the layout. A tap moves the focus there directly (no d-pad walking); the game redraws the hand and
// the help text.
static const Prompt bdetail_prompts[]={ {836.f,957.f,BTN_PLUS}, {1011.f,1113.f,BTN_X}, {1171.f,1264.f,BTN_B} };
// Monster page 4 (Inventory / Barrier / Barrier Ability / Latent Abilities, each section under its own header): rows at hand y
// 0 / 86.7 / 174 / 261.3, then 34.67 apart; page 5 (Staggering Blow): one row at the top
static float bd_row(int page,int r){ if(page==0) return r<2 ? 40.f*(float)r : 128.f+35.2f*(float)(r-2);
  if(page==4) return r<3 ? 87.f*(float)r : 261.3f+34.67f*(float)(r-3); return 35.2f*(float)r; }
#define BD_GRAPH 0x9ab60
typedef struct { u32 off; signed char r,c; float x0,x1,y0,y1; } BdItem;   // y0/y1 relative to the list's y origin (720p)
static const BdItem bd_items[]={
  {0x7c7c8,0,0, 35.f,350.f,  5.7f, 45.7f},   // house, then the crest icons (column set below)
  {0x7c7c8,1,0, 40.f,135.f, 45.7f, 78.7f}, {0x7c7c8,1,1,136.f,330.f, 42.7f, 78.7f},
  {0x7c7c8,2,0, 40.f,195.f, 80.7f,112.7f}, {0x7c7c8,2,1,196.f,280.f, 80.7f,112.7f},
  {0x7c7c8,3,0, 40.f,280.f,113.7f,145.7f}, {0x7c7c8,3,1,281.f,330.f,113.7f,145.7f},
  {0x7c7c8,3,2,331.f,394.f,105.f,155.f}, {0x7c7c8,3,2,395.f,460.f,105.f,145.7f},   // adjutant portrait, Formation State icon (column set below)
  {0x80d60,0,0, 35.f,172.f,  1.7f, 78.7f}, {0x80d60,0,1,173.f,313.f,  1.7f, 78.7f}, {0x80d60,0,2,314.f,455.f,  1.7f, 78.7f},
  {0x80d60,1,0, 35.f,172.f, 86.7f,118.f }, {0x80d60,1,1,173.f,313.f, 86.7f,118.f }, {0x80d60,1,2,314.f,455.f, 86.7f,118.f },
  {0x80d60,2,0, 35.f,172.f,118.7f,152.f }, {0x80d60,2,1,173.f,455.f,118.7f,152.f },
  {0x7e350,0,0, 35.f,245.f, 29.4f, 62.7f}, {0x7e350,0,1,246.f,455.f, 29.4f, 62.7f},
  {0x7e350,1,0, 35.f,245.f, 62.7f, 96.f }, {0x7e350,1,1,246.f,455.f, 62.7f, 96.f },
  {0x7e350,2,0, 35.f,245.f, 96.f ,129.4f}, {0x7e350,2,1,246.f,455.f, 96.f ,129.4f},
  {0x7e350,3,0, 35.f,245.f,129.4f,163.f }, {0x7e350,3,1,246.f,455.f,129.4f,163.f },
  {0x81948,-1,0, 45.f,100.f, 30.f, 85.f}, {0x81948,-1,1,101.f,142.f, 30.f, 85.f}, {0x81948,-1,2,143.f,184.f, 30.f, 85.f},
  {0x81948,-1,3,185.f,228.f, 30.f, 85.f}, {0x81948,-1,4,236.f,277.f, 30.f, 85.f}, {0x81948,-1,5,278.f,319.f, 30.f, 85.f},
  {0x81948,-1,6,320.f,361.f, 30.f, 85.f}, {0x81948,-1,7,362.f,403.f, 30.f, 85.f}, {0x81948,-1,8,404.f,450.f, 30.f, 85.f},
  {0x7ff70,0,0,843.f,923.f, 33.7f, 84.7f}, {0x7ff70,0,1,924.f,1004.f, 33.7f, 84.7f}, {0x7ff70,0,2,1005.f,1085.f, 33.7f, 84.7f},
  {0x7ff70,0,3,1086.f,1166.f, 33.7f, 84.7f}, {0x7ff70,0,4,1167.f,1247.f, 33.7f, 84.7f},
  {0x7ff70,1,0,843.f,923.f, 85.7f,136.7f}, {0x7ff70,1,1,924.f,1004.f, 85.7f,136.7f}, {0x7ff70,1,2,1005.f,1085.f, 85.7f,136.7f},
  {0x7ff70,1,3,1086.f,1166.f, 85.7f,136.7f},
};
static u64 bd_graph(void){ u64 b=battle(); if(!b || RDS32(b+0x3cc0)!=16) return 0; u64 h=hud(); if(!h) return 0; u64 gr=h+BD_GRAPH;
  if(RD64(gr)!=g_main+0x19ee750) return 0; u32 n=RD32(gr+0x34); if(n<1||n>8) return 0; return gr; }
static int bdet_det_now(void){ u64 gr=bd_graph(); return gr && *(volatile u8*)(gr+0x30); }
static u64 bd_node_of(u64 gr,u64 L){ u32 n=RD32(gr+0x34); for(u32 k=0;k<n;k++){ u64 nd=gr+0x38+(u64)k*0x30; if(RD64(nd)==L) return nd; } return 0; }
// the right-hand list: rows from the hand when it is there, else from the list's origin (Inventory page 0 / Combat Arts page 2 /
// monster pages 4, 5)
// Inventory page 0: after the two equipped rows the "Items" header, then a "Magic" header (52 px) before the first spell, also when there
// are no items (an empty Items section is drawn: enemy Dark Bishop, Banshee / Circe Staff | Items | Magic: Miasma, Death). The entries are u16 at +0x84 (inventory slots < 0x100, magic ids >= 0x100), as in the battle Items list (Dorothea: Fire,
// Iron Shield | Items: 4 | Magic: Thunder ...: taps on the spells landed a row or two off).
// Monster page 4: bd_row has one Barrier Ability row; a second one (Golem: Vital Defense, Anti-Magic Armor) is row 3, right under the first
// instead of 52.6 px lower under the Latent Abilities header, and the Latent rows then have the same y by index. The abilities are the
// bytes +0x44 / +0x45 of main+0x40e060(unit) (read-only lookup; 0xff / 0xf0 = none; unit = list+0x6ac8), as in the game's own row code.
static int bd_ab_set(u32 b){ return b!=0xff && b!=0xf0; }
static float bd_row_ex(u64 L,s32 page,s32 cnt,int r){
  if(page==4){ if(r!=3) return 0.f; u64 u=RD64(L+0x6ac8); if(!mem_ok(u)) return 0.f; u64 d=((u64(*)(u64))(g_main+0x40e060))(u);
    return (mem_ok(d) && bd_ab_set(*(volatile u8*)(d+0x44)) && bd_ab_set(*(volatile u8*)(d+0x45))) ? -52.6f : 0.f; }
  if(page!=0 || r<2) return 0.f;
  int ms=-1; for(int i=2;i<cnt&&i<40;i++){ if(*(volatile u16*)(L+0x84+2*i)>=0x100){ ms=i; break; } }
  return (ms>=2 && r>=ms) ? 52.f : 0.f; }
// A scrolled list (s16 +0x6a > 0) moves its rows up by a distance that isn't stored (it stops at the end of the list: Annette's 9 rows
// scroll 20.5 px), while page 0's two equipped rows stay put. It is measured from the hand on a scrolling row; with the hand elsewhere
// (another panel) the row can't be placed: -2 (bdetail_tap moves the focus here first and replays the tap).
static int bd_inv_row(u64 g,u64 L,int foc,float x,float y){
  if(x<835.f||x>1215.f) return -1; s32 page=RDS32(L+0xfe0), idx=RDS32(L+0x4c), cnt=RDS32(L+0x54); if(cnt<=0||cnt>20) return -1;
  float base= (page==0||page==4||page==5) ? RDF(L+0x30)/1.5f+33.f : page==2 ? RDF(L+0x30)/1.5f+74.3f : -1.f, sc=0.f;
  int scrolled=(short)*(volatile u16*)(L+0x6a)>0;
  if(foc && idx>=0 && idx<cnt){ float hy=RDF(g+0x30)/1.5f-1.f-bd_row(page,idx)-bd_row_ex(L,page,cnt,idx);
    if(base<0.f) base=hy; else if(page!=0 || idx>=2) sc=base-hy; }
  else if(base<0.f) return -1;
  else if(scrolled) return (y>=base-20.f && y<=base+420.f) ? -2 : -1;
  int best=-1; float bd=1e9f; for(int r=0;r<cnt;r++){ float d=y-(base+bd_row(page,r)+bd_row_ex(L,page,cnt,r)-((page!=0||r>=2)?sc:0.f)); if(d<0)d=-d; if(d<bd){ bd=d; best=r; } }
  return bd<=19.f ? best : -1; }
// Battalion page (page owner+0xfe0 = 3): rows at the list's y origin (+0x30, 1080p) + 74.3 / 115 / 147 (hand y, 720p); row 1 has
// two items (Battalion Level x 870, Exp x 1053). Returns the row, -1 for none.
static int bd_bat_hit(u64 L,float x,float y,int*oc){ if(x<845.f||x>1215.f) return -1; float ry=y-RDF(L+0x30)/1.5f; *oc=0;
  if(ry>=58.f&&ry<94.f) return 0; if(ry>=94.f&&ry<131.f){ *oc= x>=1040.f; return 1; } if(ry>=131.f&&ry<166.f) return 2; return -1; }
static int bdetail_tap(u64 g,float x,float y){
  u64 gr=bd_graph(); if(!gr || !hand_on(g)) return 0; u64 h=hud(), o=RD64(g+G_HAND_OWNER); if(!bd_node_of(gr,o)) return 0;
  if(prompt_tap(x,y,bdetail_prompts,NP(bdetail_prompts))) return 1;
  if(y>=42.f&&y<=82.f){ if(x>=840.f&&x<=915.f){ press(BTN_ZL); return 1; } if(x>=1150.f&&x<=1230.f){ press(BTN_ZR); return 1; } }
  int det=*(volatile u8*)(gr+0x30)!=0;
  u64 L=0; int r=-1,c=0;
  int bpage=0;   // Battalion page: a 3x2 grid (rows: battalion / Battalion Level, Exp / gambit), help text only in Details
  { u64 inv=h+0x830b8; if(is_widget(inv,0x19f9678) && bd_node_of(gr,inv)){
      if(RDS32(inv+0xfe0)==3){ int cc=0, rr=bd_bat_hit(inv,x,y,&cc); if(rr>=0 && rr<RDS32(inv+0x54)){ L=inv; r=rr; c=cc; bpage=1; } }
      else { int rr=bd_inv_row(g,inv,o==inv,x,y); if(rr>=0){ L=inv; r=rr; }
        else if(rr==-2){ if(rt_depth==0 && *(volatile u8*)(gr+0x30)){   // scrolled: hand onto the list first, then place the row from it
            if(mem_ok(o)) *(volatile u8*)(o+8)&=(u8)~0x0c; *(volatile u8*)(inv+8)|=0x0c;
            *(volatile u64*)(g+G_HAND_OWNER)=inv; *(volatile u64*)(gr+0x10)=bd_node_of(gr,inv); list_sound(inv);
            retap_later(x,y,4); tm_log2("bdetail scrolled list",(s64)x,(s64)y); }
          return 1; } } } }
  // Status effects (gambit hit, stat debuffs ...): the top panel list gets an extra row 0 with one icon per status, drawn above the name
  // bar (icons ~28 px, first at x 39-67, y = origin - 65 .. - 39). The other rows move down by one index; per-row column counts at +0x53c.
  { u64 W=h+0x7c7c8; if(!L && widget_vis(W) && bd_node_of(gr,W) && RDS32(W+0x54)==5){ float oy=RDF(W+0x30)/1.5f; int nc=*(volatile u8*)(W+0x53c);
      if(y>=oy-68.f && y<=oy-36.f && x>=35.f && nc>=1 && nc<=13){ int cc=(int)((x-37.f)/31.f); if(cc>=0 && cc<nc){ L=W; r=0; c=cc; } else return 1; } } }
  if(!L) for(unsigned i=0;i<sizeof(bd_items)/sizeof(bd_items[0]);i++){ const BdItem*e=&bd_items[i]; u64 W=h+e->off;
    if(x<e->x0||x>e->x1 || !widget_vis(W) || !bd_node_of(gr,W)) continue;
    float oy=RDF(W+0x30)/1.5f; if(y<oy+e->y0||y>oy+e->y1) continue;
    int er=e->r;
    if(e->off==0x7c7c8){ if(RDS32(W+0x54)==5) er++;                     // status row on top
      int nc=*(volatile u8*)(W+0x53c+er); if(nc>=1 && nc<=13 && e->c>=nc) continue; }   // e.g. no battalion: one item in the crest row
    if(er>=0 && er>=RDS32(W+0x54)) continue; if(e->c>=RDS32(W+0x58) && e->c>0) continue;
    L=W; r=er; c=e->c;
    // the house row: the house, then one icon per crest, right-aligned (the last at x 314, 34 apart: Edelgard's two at 280 / 314)
    if(e->off==0x7c7c8 && e->r==0){ int nc=*(volatile u8*)(W+0x53c+er); for(int i=1;i<nc && i<4;i++){ float cx=314.f-34.f*(float)(nc-1-i); if(x>=cx-17.f && x<cx+17.f) c=i; } }
    // the class row after class / movement type: the adjutant (third, when there is one) and the Formation State icon (always last):
    // Byleth with Ingrid 4 items, enemy Solon 3 (formation third)
    if(e->off==0x7c7c8 && e->r==3 && e->x0>=395.f) c=*(volatile u8*)(W+0x53c+er)-1;
    // monsters: the HP row has 3 items (HP, the barrier gem, Mv) and the class row 3 (class, two movement icons)
    if(e->off==0x7c7c8 && (e->r==2||e->r==3) && e->c<2 && *(volatile u8*)(W+0x53c+er-(e->r-2))==3 && *(volatile u8*)(W+0x53c+er)==3){
      c= e->r==2 ? (x<176.f?0: x<209.f?1:2) : (x<266.f?0: x<293.f?1:2); }
    break; }
  if(!L) return 1;
  if(!det && L==o && L==h+0x830b8 && !bpage){ if(r!=RDS32(L+0x4c)){ g_learn_hold=20; ((SetIdxFn)(g_main+0x5de5b0))(L,r); list_sound(L); } tm_log2("bdetail",r,0); return 1; }   // outside Details, the right-hand rows: SetIndex alone, straight there
  if(det && L==o && (r<0 || RDS32(L+0x4c)==r) && RDS32(L+0x50)==c) return 1;   // already there
  if(L!=o){ if(mem_ok(o)) *(volatile u8*)(o+8)&=(u8)~0x0c; *(volatile u8*)(L+8)|=0x0c;
    *(volatile u64*)(g+G_HAND_OWNER)=L; *(volatile u64*)(gr+0x10)=bd_node_of(gr,L); }
  if(RDS32(L+0x58)>1) set_col(L,c);
  if(r>=0) ((SetIdxFn)(g_main+0x5de5b0))(L,r);
  g_learn_hold=20;
  // outside Details the rest needs Details: the focus is moved there first (as above, which works outside Details too), then X opens
  // Details right on it (was X, then the tap replayed once Details was up: the hand first showed on the old item, then glided over)
  list_sound(L);
  if(!det){ press(BTN_X); tm_log2("bdetail details",(s64)(L-h),r*10+c); return 1; }
  tm_log2("bdetail item",(s64)(L-h),r*10+c); return 1; }
