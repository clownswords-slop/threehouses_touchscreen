// ===== Touch control logic =====
#define BTN_A (1ul<<0)
#define BTN_B (1ul<<1)
#define BTN_L (1ul<<12)
#define BTN_U (1ul<<13)
#define BTN_R (1ul<<14)
#define BTN_D (1ul<<15)
#define BTN_X (1ul<<2)
#define BTN_Y (1ul<<3)
#define BTN_LB (1ul<<6)
#define BTN_RB (1ul<<7)
#define BTN_ZL (1ul<<8)
#define BTN_ZR (1ul<<9)
#define BTN_PLUS (1ul<<10)
#define BTN_MINUS (1ul<<11)

static u64 g_main;           // main module base
static u64 g_poll;           // poll counter (primary npad)
static u32 g_primary_id=0xffffffff;
#ifdef TM_DEBUG
static int tlog=1;
#else
static int tlog=0;
#endif

#define RD32(a) (*(volatile u32*)(a))
#define RDS32(a) (*(volatile s32*)(a))
#define RD64(a) (*(volatile u64*)(a))
#define RDF(a) (*(volatile float*)(a))

static int mem_ok(u64 a){ MemInfo mi; u32 pi; if(a<0x1000000) return 0; if(svcQueryMemory(&mi,&pi,a)) return 0; return (mi.perm&1) && mi.type!=0; }

static void find_main(u64 lr){ MemInfo mi; u32 pi; if(svcQueryMemory(&mi,&pi,lr)) return; // text segment region of main
  g_main = mi.addr; }

static u64 battle(void){ if(!g_main) return 0; u64 p=RD64(g_main+0x1aa77a8); return p; }

// ---- action queue ----
// Each step: buttons held for 'hold' polls, then released for 'gap' polls.
typedef struct { u64 btn; int hold, gap; } Step;
static Step q[64]; static int qh, qt; static int q_phase, q_cnt; static u64 q_cur;
static void q_push(u64 b,int hold,int gap){ int n=(qt+1)&63; if(n==qh) return; q[qt].btn=b; q[qt].hold=hold; q[qt].gap=gap; qt=n; }
static void press(u64 b);
static int q_busy(void){ return qh!=qt || q_phase!=0; }
static void q_clear(void){ qh=qt=0; q_phase=0; q_cur=0; }
static u64 q_tick(void){ // call once per poll, returns buttons to OR in
  if(q_phase==0){ if(qh==qt) return 0; q_cur=q[qh].btn; q_cnt=q[qh].hold; q_phase=1; }
  if(q_phase==1){ if(q_cnt-->0) return q_cur; q_phase=2; q_cnt=q[qh].gap; }
  if(q_phase==2){ if(q_cnt-->0) return 0; q_phase=0; qh=(qh+1)&63; return q_tick(); }
  return 0;
}

// ---- instant list selection ----
// All the game's list menus share one cursor base class: +0x4c index, +0x54 count, s16 +0x6a first visible row.
// main+0x5de5b0 is its SetIndex(list, idx): stores the index and fixes up the scroll position, no animation.
// We put the index right next to the target and then send one real d-pad step, so the game still runs its own
// "selection changed" code (hand pointer, help text, sound) but the cursor only travels one row.
typedef void (*SetIdxFn)(u64 w,s32 idx);
static int g_jump_dir; // debug: 0 = jump+step, 1 = SetIndex only
static int g_learn_hold; // polls during which pitch learning is paused (our own SetIndex moves the index before the indicator)
static void list_jump(u64 w,int it,int then_a){
  g_learn_hold=20;
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54);
  if(it>=0&&it<cnt&&it!=idx){
    if(g_jump_dir==1){ ((SetIdxFn)(g_main+0x5de5b0))(w,it); }
    else { int pre= it>idx ? it-1 : it+1;
      if(pre!=idx) ((SetIdxFn)(g_main+0x5de5b0))(w,pre);
      q_push(it>idx?BTN_D:BTN_U,2,4); }
  }
  if(then_a) q_push(BTN_A,2,3);
}
// 2-D lists: main+0x5de6f0 is the column counterpart of SetIndex (stores +0x50, fixes the column scroll +0x68)
static void set_col(u64 w,s32 c){ ((SetIdxFn)(g_main+0x5de6f0))(w,c); }
static void grid_jump(u64 w,int r,int c,int then_a){
  g_learn_hold=20;
  s32 idx=RDS32(w+0x4c), cidx=RDS32(w+0x50);
  if(r!=idx){ if(c!=cidx) set_col(w,c); list_jump(w,r,then_a); return; }
  if(c!=cidx){ int pre= c>cidx ? c-1 : c+1; if(pre!=cidx) set_col(w,pre); q_push(c>cidx?BTN_R:BTN_L,2,4); }
  if(then_a) q_push(BTN_A,2,3);
}
// ---- double tap to confirm ----
// Item and unit lists: the first tap on a row only selects it; a second tap on the same row confirms (A). The "armed" row is
// remembered as (address of the index, value[, column address, value]) plus the hand owner; it is dropped as soon as the
// selection or the owner changes (keyboard input, another screen), so a stale arm never confirms.
static u64 cmn_ui(void);
static void press(u64 b);
static u64 dt_key, dt_key2, dt_own; static s32 dt_val, dt_val2; static int dt_on, dt_grace;
static int dt_match(void){ return RDS32(dt_key)==dt_val && (!dt_key2 || RDS32(dt_key2)==dt_val2); }
static void dt_arm(u64 key,s32 val,u64 key2,s32 val2){ u64 g=cmn_ui(); dt_on=1; dt_key=key; dt_val=val; dt_key2=key2; dt_val2=val2;
  dt_own= (g && *(volatile u8*)(g+0x50)) ? RD64(g+0x58) : 0; dt_grace=45; }
static int dt_same(u64 key,s32 val,u64 key2,s32 val2){ return dt_on && dt_key==key && dt_val==val && dt_key2==key2 && dt_val2==val2; }
static void dt_tick(void){ if(!dt_on || q_busy()) return;
  u64 g=cmn_ui(); if(dt_own && g && RD64(g+0x58)!=dt_own){ dt_on=0; return; }
  if(dt_grace>0){ if(dt_match()) dt_grace=0; else if(--dt_grace==0) dt_on=0; return; }
  if(!dt_match()) dt_on=0; }
// list row: select, or confirm when this row was already picked by a tap (also while its step is still on its way)
// The row that is already selected (the first row when a list opens, or one reached with the d-pad) confirms on one tap, like
// the first attack target: its info already shows. Only a row that still has to be selected takes a second tap.
static void list_pick(u64 w,int it){
  if(dt_same(w+0x4c,it,0,0)){ dt_on=0; if(RDS32(w+0x4c)!=it) list_jump(w,it,1); else press(BTN_A); return; }
  if(RDS32(w+0x4c)==it && !q_busy()){ dt_on=0; press(BTN_A); return; }
  if(RDS32(w+0x4c)!=it) list_jump(w,it,0); dt_arm(w+0x4c,it,0,0); }
static void grid_pick(u64 w,int r,int c){
  if(dt_same(w+0x4c,r,w+0x50,c)){ dt_on=0; if(RDS32(w+0x4c)!=r||RDS32(w+0x50)!=c) grid_jump(w,r,c,1); else press(BTN_A); return; }
  if(RDS32(w+0x4c)==r && RDS32(w+0x50)==c && !q_busy()){ dt_on=0; press(BTN_A); return; }
  if(RDS32(w+0x4c)!=r||RDS32(w+0x50)!=c) grid_jump(w,r,c,0); dt_arm(w+0x4c,r,w+0x50,c); }
// ---- instant cursor placement: game's own SetCursor(battle, x, y, snap) at main+0x9e800 ----
typedef void (*SetCursorFn)(u64 battle, u32 x, u32 y, int snap);
static void set_cursor_raw(int x,int y,int snap){ u64 b=battle(); if(!b||!g_main) return; SetCursorFn f=(SetCursorFn)(g_main+0x9e800); f(b,(u32)x,(u32)y,snap); }
static int set_cursor_instant(int x,int y){ u64 b=battle(); if(!b||!g_main) return 0; set_cursor_raw(x,y,1); return RDS32(b+4)==x&&RDS32(b+8)==y; }
// wait for cursor to arrive at (x,y) then press A
static int wa_on, wa_x, wa_y, wa_n; static s32 wa_ph;
static void wait_then_a(int x,int y){ u64 b=battle(); wa_on=1; wa_x=x; wa_y=y; wa_n=0; wa_ph= b ? RDS32(b+0x3cc0) : -1; }
static void move_confirmed(void);
static void wa_tick(void){ if(!wa_on||q_busy()) return; u64 b=battle(); if(!b){wa_on=0;return;}
  if(RDS32(b+0x3cc0)!=wa_ph){ wa_on=0; return; }   // the screen changed (e.g. target selection opened): no stray A there
  if(RDS32(b+4)==wa_x&&RDS32(b+8)==wa_y){ if(wa_n>=1){ wa_on=0; move_confirmed(); q_push(BTN_A,2,3);} else wa_n++; return; } if(++wa_n>45){ wa_on=0; } }
// ---- instant target selection (attack / talk / heal targets) ----
static u64 cell_at(int x,int y){ u64 g=RD64(g_main+0x1aa7cc8); if(!g) return 0; u64 base=RD64(g); if(!base||x<0||y<0||x>31||y>31) return 0; return base+(u64)y*0x600+(u64)x*0x30; }
// The game's own "target this unit" (main+0x181c90, what the d-pad calls in target mode): (ctl, unit, 1, -, set, x, y) checks
// that the unit is a valid target for the current action (attack, heal, trade, gambit ...) and, with set = 1, makes it the target
// (battle+0x3ce0, its tile, the cursor, facing). ctl = *(battle+0x3bb8), the map controller (vt main+0x19f5de0); x, y = -1 for
// one-tile units. Ally targets (trade, heal) have no 0x20 flag on their cell, so the flag test alone missed them and the cursor
// chase fell back to d-pad steps, which hop between the targets in the game's own order (two of them, back and forth).
static void tm_log2(const char*m,s64 a,s64 b);
static int g_tgt_known;   // target_select: 1 when the game's own check answered (a 0 then means "not a target")
typedef u32 (*TgtFn)(u64 ctl,u64 unit,u32 enemy,u32 unused,u32 set,s32 x,s32 y);
// Trade (controller sub-state ctl+8 = 14) has its own d-pad handler (main+0x180f70): the candidates come from main+0xca450(list, acting)
// (main+0xca560 for action kind 0x18), a small linked list {+8 node storage, +0x10 head, +0x20 capacity, +0x24 count; nodes of 0x20
// bytes {unit, next, prev, -}}; picking one writes the target, its tile, the cursor and the cell refs, then refreshes the trade panel
// (HUD+0x1cc48: vtable +0x1a8 (unit), +0xd0 (unit+0x87, 0, 1), main+0x5dd530).
static u64 hud(void);
typedef struct { u64 alloc, store, head, tail; u32 cap, cnt; } GList;
static GList g_tl; static u8 g_tl_store[100*0x20];
static int trade_has(u64 b,u64 unit){ u64 act=RD64(b+0x3b60); if(!mem_ok(act)) return 0;
  ((void(*)(GList*))(g_main+0x5be510))(&g_tl); ((void(*)(GList*,void*,u32))(g_main+0x5be630))(&g_tl,g_tl_store,100);
  ((void(*)(GList*,u64))(g_main+(RDS32(b+0x3cdc)==0x18?0xca560:0xca450)))(&g_tl,act);
  int n=0; for(u64 nd=g_tl.head; nd && n<100; nd=RD64(nd+8), n++) if(RD64(nd)==unit) return 1;
  return 0; }
static void trade_set(u64 b,u64 unit){
  s32 x=*(volatile u8*)(unit+0xf2), y=*(volatile u8*)(unit+0xf3); u64 c=cell_at(x,y); if(!c) return;
  *(volatile u64*)(b+0x3ce0)=unit; *(volatile s32*)(b+0x3ce8)=x; *(volatile s32*)(b+0x3cec)=y; *(volatile s32*)(b+4)=x; *(volatile s32*)(b+8)=y;
  u64 c10=RD64(c+0x10), c18=RD64(c+0x18);
  *(volatile u64*)(b+0x3b38)=c18; *(volatile u64*)(b+0x3b30)=c10; *(volatile u64*)(b+0x3b58)=c18; *(volatile u64*)(b+0x3b50)=c10;
  *(volatile u64*)(b+0x3b48)=c18; *(volatile u64*)(b+0x3b40)=c10; *(volatile s32*)(b+0x3d04)=0;
  u64 h=hud(); if(!h) return; u64 w=h+0x1cc48; u64 vt=RD64(w); if(!mem_ok(vt)) return;
  ((void(*)(u64,u64))RD64(vt+0x1a8))(w,unit);
  ((void(*)(u64,u32,u32,u32))RD64(vt+0xd0))(w,*(volatile u8*)(unit+0x87),0,1);
  ((void(*)(u64))(g_main+0x5dd530))(w); }
// 1 = valid target (set when asked), 0 = not a target for this action, -1 = can't tell
static int target_game(u64 b,u64 unit,int set){
  u64 ctl=RD64(b+0x3bb8); if(!mem_ok(ctl) || RD64(ctl)!=g_main+0x19f5de0) return -1;
  s32 ss=RDS32(ctl+8), k=RDS32(b+0x3cdc);
  if(ss==14){ if(!trade_has(b,unit)) return 0; if(set) trade_set(b,unit); return 1; }
  // attack / gambit (sub-state 11), heal & co (5): the d-pad goes through main+0x180290 -> main+0x181c90. Action kinds 2, 8 and 23
  // take their targets from lists built elsewhere: there that function sets any unit without checking, so leave those to the cell flags.
  if(ss!=11 && ss!=5) return -1;
  if(k==2||k==8||k==23||k<0||k>0x1d) return -1;
  return (((TgtFn)(g_main+0x181c90))(ctl,unit,(ss==5&&k==0xf)?0u:1u,0,(u32)set,-1,-1)&0xff)!=0; }
static int target_select(int x,int y){
  u64 b=battle(); u64 c=cell_at(x,y); if(!b||!c) return 0;
  u64 unit=RD64(c); g_tgt_known=0; if(!unit) return 0;
  { int r=target_game(b,unit,1); if(r>=0) g_tgt_known=1;
    if(r==1){ u64 ctl=RD64(b+0x3bb8); if(RDS32(ctl+8)!=14) set_cursor_instant(x,y); tm_log2("target game",x*100+y,RDS32(ctl+8)); return 1; }
    if(r==0){ tm_log2("not a target",x*100+y,0); return 0; } }
  if(!(*(volatile u8*)(c+0x26)&0x20)) return 0;   // not a highlighted target
  *(volatile u64*)(b+0x3ce0)=unit; *(volatile u32*)(b+0x3ce8)=(u32)x; *(volatile u32*)(b+0x3cec)=(u32)y;
  set_cursor_instant(x,y);
  return 1;
}
// ---- cursor chase ----
static int chase_maxsteps=80, chase_maxfail=6;
static int chase_on, chase_tx, chase_ty, chase_steps, chase_fail, chase_then_a, chase_lastx, chase_lasty, chase_wait;
static u64 chase_lastbtn;
// learned mapping: dpad -> (dx,dy)
static int dmap_dx[4]={0,0,-1,1}, dmap_dy[4]={-1,1,0,0}; // U D L R
static const u64 dbtn[4]={BTN_U,BTN_D,BTN_L,BTN_R};

static void cursor_get(int*x,int*y){ u64 b=battle(); *x=RDS32(b+4); *y=RDS32(b+8); }

// The chase walks the cursor with d-pad steps, so it only makes sense where a step moves the cursor one tile: free cursor (0),
// unit selected (1), prep map (30). In target selection (2) a d-pad step jumps to the next target in the game's order, so a chase
// there hopped between the targets on its own (up to 80 steps) and stopped on whichever one it happened to reach; it also
// "learned" those jumps as the d-pad directions. A chase therefore never starts in phase 2 and stops when the phase changes
// (e.g. the A at the end of a move turns into target selection while steps are still queued).
static s32 chase_ph;
static int chase_phase_ok(s32 ph){ return ph==0 || ph==1 || ph==30; }
static void chase_start(int tx,int ty,int then_a){ u64 b=battle(); s32 ph= b ? RDS32(b+0x3cc0) : -1; if(!chase_phase_ok(ph)){ chase_on=0; tm_log2("chase refused",ph,0); return; }
  chase_ph=ph; chase_maxsteps=80; chase_maxfail=6; chase_on=1; chase_tx=tx; chase_ty=ty; chase_steps=0; chase_fail=0; chase_then_a=then_a; chase_wait=0; chase_lastbtn=0; cursor_get(&chase_lastx,&chase_lasty); }

// Target selection fallback: one Right step at a time (each moves to the next target in the game's order), checked after every
// step; stops on the tapped tile, when the cursor is back where it started (a full round: not a target), after 12 steps or when
// the phase changes. Never presses A.
static int tc_on, tc_tx, tc_ty, tc_sx, tc_sy, tc_n, tc_wait;
static void tcycle_start(int tx,int ty){ chase_on=0; tc_on=1; tc_tx=tx; tc_ty=ty; cursor_get(&tc_sx,&tc_sy); tc_n=0; tc_wait=0; tm_log2("target cycle",tx*100+ty,tc_sx*100+tc_sy); }
static void tcycle_tick(void){
  if(!tc_on || q_busy()) return; u64 b=battle(); if(!b || RDS32(b+0x3cc0)!=2){ tc_on=0; return; }
  if(tc_wait>0){ tc_wait--; return; }   // let the game move the cursor after the step
  int cx,cy; cursor_get(&cx,&cy);
  if(cx==tc_tx&&cy==tc_ty){ tc_on=0; tm_log2("target cycle done",tc_n,0); return; }
  if(tc_n>0 && cx==tc_sx && cy==tc_sy){ tc_on=0; tm_log2("target cycle: not a target",tc_n,0); return; }
  if(tc_n>=12){ tc_on=0; tm_log2("target cycle gave up",tc_n,0); return; }
  tc_n++; tc_wait=3; q_push(BTN_R,1,2); }
static void chase_tick(void){
  if(!chase_on || q_busy()) return;
  { u64 b=battle(); if(!b || RDS32(b+0x3cc0)!=chase_ph){ chase_on=0; chase_lastbtn=0; tm_log2("chase stopped: phase",b?RDS32(b+0x3cc0):-1,chase_ph); return; } }
  int cx,cy; cursor_get(&cx,&cy);
  if(chase_lastbtn){ // learn from last press
    int dx=cx-chase_lastx, dy=cy-chase_lasty;
    if(dx||dy){ for(int i=0;i<4;i++) if(dbtn[i]==chase_lastbtn){ if((dx>=-1&&dx<=1)&&(dy>=-1&&dy<=1)&&(dx==0||dy==0)){ dmap_dx[i]=dx; dmap_dy[i]=dy; } } chase_fail=0; }
    else chase_fail++;
    chase_lastbtn=0;
  }
  chase_lastx=cx; chase_lasty=cy;
  if(cx==chase_tx && cy==chase_ty){ chase_on=0; if(chase_then_a){ move_confirmed(); q_push(BTN_A,2,3); } return; }
  if(chase_steps++>chase_maxsteps || chase_fail>chase_maxfail){ chase_on=0; if(tlog){ Buf b={0}; bstr(&b,"[touchmod] chase gave up"); bflush(&b);} return; }
  int need_x=chase_tx-cx, need_y=chase_ty-cy; int best=-1, bs=0;
  for(int i=0;i<4;i++){ int s=dmap_dx[i]*need_x + dmap_dy[i]*need_y; if(s>bs){ bs=s; best=i; } }
  if(best<0){ chase_on=0; return; }
  // alternate axis when both needed? choose best score already
  chase_lastbtn=dbtn[best]; q_push(dbtn[best],1,2);
}

static int tile_has_unit(int x,int y){ u64 g=RD64(g_main+0x1aa7cc8); if(!g) return 0; u64 base=RD64(g); if(!base) return 0; if(x<0||y<0||x>31||y>31) return 0; u64 c=base+(u64)y*0x600+(u64)x*0x30; return RD64(c)!=0; }
// ---- camera / picking ----
static u64 cam_obj(void){ u64 p=RD64(g_main+0x1ab8480); if(!p || !mem_ok(p)) return 0; float asp=RDF(p+0x1bc); if(!(asp>1.5f&&asp<2.0f)) return 0; return p; }

// ---- hide the map cursor while playing by touch ----
// main+0xa2810 = the game's own ShowCursor(battle, show): creates/destroys the cursor effects
// (100_BMAP_cursor_*) and stores the state at battle+0x3d5d. The game calls it on state changes,
// so while in touch mode we hide it again whenever it comes back, and restore it on real button input.
typedef void (*CurShowFn)(u64 b,int show);
static int g_touch_mode, g_cur_hidden, g_move_hide, g_move_ph;
// called when a touch confirms the destination of a move (A pressed in phase 1)
static void move_confirmed(void){ u64 b=battle(); if(!b) return; s32 ph=RDS32(b+0x3cc0); if(ph!=1) return; g_move_hide=1; g_move_ph=0; }
static void cursor_hide_tick(void){
  u64 b=battle(); if(!b||!g_main) return;
  s32 ph=RDS32(b+0x3cc0);
  volatile u8*shown=(volatile u8*)(b+0x3d5d);
  // Hidden only from the tap on a destination tile until the unit has walked there. Phase 1 = unit selected,
  // 8 = unit walking, 2 = action menu (the game hides the cursor itself in 8 and 2).
  if(g_move_hide){
    if((ph==1||ph==8) && ++g_move_ph<300){ if(*shown){ ((CurShowFn)(g_main+0xa2810))(b,0); g_cur_hidden=1; } return; }
    g_move_hide=0; }
  if(g_cur_hidden){ g_cur_hidden=0; if(!*shown && (ph==0||ph==1) && RDS32(b+0x3b20)>0 && cam_obj()) ((CurShowFn)(g_main+0xa2810))(b,1); }
}
static int g_force_tile, g_ftx, g_fty;
// Tap -> tile: cast the tap through the camera and intersect it with the ground plane (y = look-at height).
// The camera is tilted, so far rows look shorter on screen; picking by nearest tile centre was biased
// towards the row above. Solving the plane intersection gives the exact tile under the finger.
static float g_cm[16], g_ch; static int g_cfrozen;   // camera frozen at the start of a cursor drag
static void cam_freeze(int on){ g_cfrozen=0; if(!on) return; u64 c=cam_obj(); if(!c) return;
  for(int i=0;i<16;i++) g_cm[i]=RDF(c+0x170+i*4); g_ch=RDF(c+0x94); g_cfrozen=1; }
static int pick_plane(float tx,float ty,float*wx,float*wz){
  u64 c=cam_obj(); if(!c) return 0;
  volatile float*m=(volatile float*)(c+0x170); float h=RDF(c+0x94);
  if(g_cfrozen){ m=(volatile float*)g_cm; h=g_ch; }
  float nx=tx/640.f-1.f, ny=1.f-ty/360.f;
  // clip.x - nx*clip.w = 0 and clip.y - ny*clip.w = 0, linear in world x and z (y fixed at h)
  float a11=m[0]-nx*m[3], a12=m[8]-nx*m[11], b1=nx*(m[7]*h+m[15])-(m[4]*h+m[12]);
  float a21=m[1]-ny*m[3], a22=m[9]-ny*m[11], b2=ny*(m[7]*h+m[15])-(m[5]*h+m[13]);
  float det=a11*a22-a12*a21; if(det>-1e-12f&&det<1e-12f) return 0;
  *wx=(b1*a22-a12*b2)/det; *wz=(a11*b2-a21*b1)/det; return 1;
}
static int pick_tile_plane(float tx,float ty,int*ox,int*oy,float*fx,float*fz){
  u64 b=battle(); if(!b) return 0;
  u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return 0;
  float wx,wz; if(!pick_plane(tx,ty,&wx,&wz)) return 0;
  float gx=(wx-RDF(po))/1000.f, gz=(wz-RDF(pz))/1000.f;
  int i=(int)(gx+1000.f)-1000, j=(int)(gz+1000.f)-1000;
  int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24);
  if(maxx<=minx||maxy<=miny||maxx>64||maxy>64) return 0;
  if(i<minx) i=minx; if(i>=maxx) i=maxx-1; if(j<miny) j=miny; if(j>=maxy) j=maxy-1;
  *ox=i; *oy=j; if(fx) *fx=gx-(float)i; if(fz) *fz=gz-(float)j; return 1;
}
static int pick_tile_old(float tx,float ty,int*ox,int*oy);
// Tap -> tile by hit-testing the tile boxes the game draws (move/attack range, cursor, grid overlay).
// The boxes sit on the terrain, so each tile is projected as its own 1000x1000 quad at the terrain height
// of its centre (from the game's own cursor height lookup).
// A tap inside several quads (a raised tile in front of a lower one) takes the one nearest the camera;
// a tap inside none takes the tile whose projected centre is nearest.
static int proj_pt(volatile float*m,float x,float y,float z,float*sx,float*sy,float*w){
  float cw=x*m[3]+y*m[7]+z*m[11]+m[15]; if(cw<1.f) return 0;
  *sx=((x*m[0]+y*m[4]+z*m[8]+m[12])/cw*0.5f+0.5f)*1280.f; *sy=(0.5f-(x*m[1]+y*m[5]+z*m[9]+m[13])/cw*0.5f)*720.f; *w=cw; return 1; }
static int g_box_hit; // debug: 1 if the last box pick was inside a quad
// main+0xf6a60 is the game's own terrain height lookup for the map cursor: (unused, float pos[4]) -> height.
// It picks the right layer for bridges/water the same way the cursor does.
typedef float (*HeightFn)(u64,float*);
static float tile_height(float x,float z,float dflt){
  float p[4]={x,0.f,z,1.f}; float h=((HeightFn)(g_main+0xf6a60))(0,p);
  if(!(h>-50000.f&&h<50000.f)) return dflt; return h; }
static int pick_tile_box(float tx,float ty,int*ox,int*oy){
  u64 b=battle(), c=cam_obj(); if(!b||!c) return 0; volatile float*m=(volatile float*)(c+0x170);
  u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return 0;
  float orgx=RDF(po), orgz=RDF(pz), h0=RDF(c+0x94);
  int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24);
  if(maxx<=minx||maxy<=miny||minx<0||miny<0||maxx>32||maxy>32) return 0;
  static const float dx[4]={-500.f,500.f,500.f,-500.f}, dz[4]={-500.f,-500.f,500.f,500.f};
  float nd=1e30f; int bi=-1,bj=-1,ni=-1,nj=-1;
  int nhit=0, hit_i[8], hit_j[8]; float hit_w[8], hit_h[8];
  for(int j=miny;j<maxy;j++) for(int i=minx;i<maxx;i++){
    float x=orgx+(i+0.5f)*1000.f, z=orgz+(j+0.5f)*1000.f, h=tile_height(x,z,h0);
    float px[4],py[4],w,sx,sy,cw; int ok=1;
    for(int k=0;k<4&&ok;k++) ok=proj_pt(m,x+dx[k],h,z+dz[k],&px[k],&py[k],&w);
    if(!ok||!proj_pt(m,x,h,z,&sx,&sy,&cw)) continue;
    int pos=0,neg=0; for(int k=0;k<4;k++){ int k1=(k+1)&3; float cr=(px[k1]-px[k])*(ty-py[k])-(py[k1]-py[k])*(tx-px[k]); if(cr>0.f) pos++; else if(cr<0.f) neg++; }
    if(!(pos&&neg) && nhit<8){ hit_i[nhit]=i; hit_j[nhit]=j; hit_w[nhit]=cw; hit_h[nhit]=h; nhit++; }
    float d=(sx-tx)*(sx-tx)+(sy-ty)*(sy-ty); if(d<nd){ nd=d; ni=i; nj=j; }
  }
  // Several boxes under the tap: normally the one nearest the camera (it is in front). But boxes of walls and fences
  // float high above the ground (e.g. 2220 vs 1000) and cover the boxes of the tiles behind them, which stay visible
  // through them; so a box more than 600 units lower behind the chosen one takes over.
  // Boxes the game actually draws (move / attack range, targets) beat undrawn tiles under the same spot.
  if(nhit>1){ int nd2=0; for(int a=0;a<nhit;a++){ u64 ce=cell_at(hit_i[a],hit_j[a]); if(ce && (*(volatile u8*)(ce+0x26)&0x23)) nd2++; }
    if(nd2>0 && nd2<nhit){ int m2=0; for(int a=0;a<nhit;a++){ u64 ce=cell_at(hit_i[a],hit_j[a]); if(ce && (*(volatile u8*)(ce+0x26)&0x23)){ hit_i[m2]=hit_i[a]; hit_j[m2]=hit_j[a]; hit_w[m2]=hit_w[a]; hit_h[m2]=hit_h[a]; m2++; } } nhit=m2; } }
  if(nhit>0){ int k=0; for(int a=1;a<nhit;a++) if(hit_w[a]<hit_w[k]) k=a;
    for(int pass=0;pass<8;pass++){ int nk=k; for(int a=0;a<nhit;a++) if(hit_w[a]>hit_w[k] && hit_h[k]-hit_h[a]>600.f && (nk==k || hit_w[a]<hit_w[nk])) nk=a; if(nk==k) break; k=nk; }
    bi=hit_i[k]; bj=hit_j[k]; }
  g_box_hit = bi>=0;
  if(bi>=0){ *ox=bi; *oy=bj; return 1; }
  if(ni>=0){ *ox=ni; *oy=nj; return 1; }
  return 0;
}
// Tap -> tile on the grid the game draws (Options -> Grid), visible or not. The drawn grid follows the terrain: its lines
// run through the tile corners at the ground height there (+100; fences / rocks are ignored, see ground_height), so raised ground (the plateau past the stairs in Red Canyon,
// ~300-600 above the camera's look-at height) is picked where it is drawn, not where a flat plane at the look-at height
// would put it. Corner heights are sampled once per map (g_gh, rebuilt when the map changes). A tap inside several
// projected tiles takes the one nearest the camera (it is drawn in front). Where the ground is at the look-at height
// this gives the same tile as the flat plane; a tap outside every tile falls back to the plane.
#define GRID_LIFT 100.f   // the grid and the cursor box are drawn ~100 above the terrain height (measured on screen)
static int g_pick_mode=1;   // 0 flat plane only, 1 grid at the ground height at the corners, 2 mean of the 4 tile centres around each corner, 3 cursor height at the corners
// The cursor's height lookup (main+0xf6a60) asks main+0x5a5d30(pos, flag, &ground, &top) for two heights: the ground (first) and the top
// of whatever stands there (second). Walkable terrain (main+0x403570(terrain) true) uses the ground; fences, rocks and pillars that only
// fliers cross use the top (2220 for the Prologue fences, 3400 for rocks, 1950 for pillars), which is where the cursor hovers on them.
// The drawn grid stays level with the ground under them, so the grid uses the ground everywhere (identical on walkable tiles); raised
// tiles are picked by their ground footprint only (a box at their top would cover the tiles behind them).
typedef void (*Layers2Fn)(float*,int,float*,float*);
static float ground_height(float x,float z,float dflt){
  float p[4]={x,0.f,z,1.f}, g=0.f, t=0.f; ((Layers2Fn)(g_main+0x5a5d30))(p,0,&g,&t);
  if(!(g>-50000.f&&g<50000.f) || g==0.f) return tile_height(x,z,dflt); return g; }
static float g_gh[33*33]; static int g_gh_ok, g_gh_mode; static float g_gh_sig[3]; static s32 g_gh_key[6];
static float gh_at(int i,int j){ return g_gh[j*33+i]; }
static int grid_heights(u64 b,float orgx,float orgz,float h0){
  int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24);
  if(maxx<=minx||maxy<=miny||minx<0||miny<0||maxx>32||maxy>32) return 0;
  s32 key[6]={minx,miny,maxx,maxy,(s32)orgx,(s32)orgz};
  float sig[3]={ tile_height(orgx+(minx+0.5f)*1000.f,orgz+(miny+0.5f)*1000.f,h0), tile_height(orgx+(maxx-0.5f)*1000.f,orgz+(maxy-0.5f)*1000.f,h0),
                 tile_height(orgx+((minx+maxx)*0.5f)*1000.f,orgz+((miny+maxy)*0.5f)*1000.f,h0) };
  int same=g_gh_ok && g_gh_mode==g_pick_mode; for(int k=0;k<6&&same;k++) if(key[k]!=g_gh_key[k]) same=0; for(int k=0;k<3&&same;k++) if(sig[k]!=g_gh_sig[k]) same=0;
  if(same) return 1;
  for(int j=miny;j<=maxy;j++) for(int i=minx;i<=maxx;i++){ float h;
    if(g_pick_mode==2){ float s=0.f; int n=0;
      for(int dj=-1;dj<=0;dj++) for(int di=-1;di<=0;di++){ int ci=i+di, cj=j+dj; if(ci<minx||cj<miny||ci>=maxx||cj>=maxy) continue;
        s+=tile_height(orgx+(ci+0.5f)*1000.f,orgz+(cj+0.5f)*1000.f,h0); n++; }
      h= n ? s/(float)n : h0; }
    else { float cx=(float)i*1000.f, cz=(float)j*1000.f;   // corners on the map's outer edge: sample just inside (outside the map the height is 0)
      if(i<=minx) cx+=2.f; if(i>=maxx) cx-=2.f; if(j<=miny) cz+=2.f; if(j>=maxy) cz-=2.f;
      h= g_pick_mode==3 ? tile_height(orgx+cx,orgz+cz,h0) : ground_height(orgx+cx,orgz+cz,h0); }
    g_gh[j*33+i]=h+GRID_LIFT; }
  for(int k=0;k<6;k++) g_gh_key[k]=key[k]; for(int k=0;k<3;k++) g_gh_sig[k]=sig[k]; g_gh_mode=g_pick_mode; g_gh_ok=1; return 1; }
static int pick_tile_grid(float tx,float ty,int*ox,int*oy){
  u64 b=battle(), c=cam_obj(); if(!b||!c) return 0;
  volatile float*m=(volatile float*)(c+0x170); float h0=RDF(c+0x94); if(g_cfrozen){ m=(volatile float*)g_cm; h0=g_ch; }
  u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return 0;
  float orgx=RDF(po), orgz=RDF(pz); if(!grid_heights(b,orgx,orgz,h0)) return 0;
  int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24);
  static const int ci[4]={0,1,1,0}, cj[4]={0,0,1,1};
  float bw=1e30f; int bi=-1,bj=-1;
  for(int j=miny;j<maxy;j++) for(int i=minx;i<maxx;i++){
    float px[4],py[4],w,hs=0.f; int ok=1;
    for(int k=0;k<4&&ok;k++){ float h=gh_at(i+ci[k],j+cj[k]); hs+=h; ok=proj_pt(m,orgx+(float)(i+ci[k])*1000.f,h,orgz+(float)(j+cj[k])*1000.f,&px[k],&py[k],&w); }
    if(!ok) continue;
    int in=0; for(int k=0;k<4;k++){ int k1=(k+1)&3; if((py[k]>ty)!=(py[k1]>ty)){ float xc=px[k]+(ty-py[k])*(px[k1]-px[k])/(py[k1]-py[k]); if(tx<xc) in^=1; } }
    if(!in) continue;
    float sx,sy,cw; if(!proj_pt(m,orgx+(i+0.5f)*1000.f,hs*0.25f,orgz+(j+0.5f)*1000.f,&sx,&sy,&cw)) continue;
    if(cw<bw){ bw=cw; bi=i; bj=j; } }
  if(bi<0) return 0; *ox=bi; *oy=bj; return 1; }
static int pick_tile_any(float tx,float ty,int*ox,int*oy){   // grid, else the flat plane
  if(g_pick_mode && pick_tile_grid(tx,ty,ox,oy)) return 1;
  return pick_tile_plane(tx,ty,ox,oy,0,0); }
static int pick_tile(float tx,float ty,int*ox,int*oy){
  if(g_force_tile){ *ox=g_ftx; *oy=g_fty; return 1; }
  if(pick_tile_any(tx,ty,ox,oy)) return 1;
  return pick_tile_old(tx,ty,ox,oy);
}
static int pick_tile_old(float tx,float ty,int*ox,int*oy){
  u64 b=battle(); u64 c=cam_obj(); if(!b||!c) return 0;
  volatile float*m=(volatile float*)(c+0x170);
  float h=RDF(c+0x94); // look-at height
  u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return 0;
  float orgx=RDF(po), orgz=RDF(pz);
  int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24);
  if(maxx<=minx||maxy<=miny||maxx>64||maxy>64) return 0;
  float best=1e30f; int bx=-1,by=-1;
  for(int j=miny;j<maxy;j++) for(int i=minx;i<maxx;i++){
    float x=orgx+(i+0.5f)*1000.f, z=orgz+(j+0.5f)*1000.f, y=h;
    float cx=x*m[0]+y*m[4]+z*m[8]+m[12];
    float cy=x*m[1]+y*m[5]+z*m[9]+m[13];
    float cw=x*m[3]+y*m[7]+z*m[11]+m[15];
    if(cw<1.f) continue;
    float sx=(cx/cw*0.5f+0.5f)*1280.f, sy=(0.5f-cy/cw*0.5f)*720.f;
    float d=(sx-tx)*(sx-tx)+(sy-ty)*(sy-ty);
    if(d<best){ best=d; bx=i; by=j; }
  }
  if(bx<0) return 0;
  *ox=bx; *oy=by; return 1;
}

// ---- menus ----
static u64 hud(void){ u64 p=RD64(g_main+0x1aca288); return (p&&mem_ok(p))?p:0; }

// list widgets inside the battle HUD: +0x21 visible, +0x4c index, +0x54 count
static int widget_vis(u64 w){ return *(volatile u8*)(w+0x21)!=0; }
static int list_tap(u64 w,float center720,float ty,const char*name){
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54);
  if(cnt<=0||cnt>12||idx<0||idx>=cnt) return 0;
  float f=(ty-center720)/66.f + (float)((cnt-1)/2);
  int it=(int)(f+0.5f+100.f)-100;
  if(it<0||it>=cnt) return 0;
  list_jump(w,it,1);
  if(tlog){ Buf b={0}; bstr(&b,"[touchmod] "); bstr(&b,name); bstr(&b," tap item "); bint(&b,it); bstr(&b," from "); bint(&b,idx); bflush(&b); }
  return 1;
}
static int tile_screen(int i,int j,float*sx,float*sy){
  u64 c=cam_obj(); if(!c) return 0; volatile float*m=(volatile float*)(c+0x170); float h=RDF(c+0x94);
  u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return 0;
  float x=RDF(po)+(i+0.5f)*1000.f, z=RDF(pz)+(j+0.5f)*1000.f, y=h;
  float cw=x*m[3]+y*m[7]+z*m[11]+m[15]; if(cw<1.f) return 0;
  *sx=((x*m[0]+y*m[4]+z*m[8]+m[12])/cw*0.5f+0.5f)*1280.f; *sy=(0.5f-(x*m[1]+y*m[5]+z*m[9]+m[13])/cw*0.5f)*720.f; return 1;
}
static int menu_tap(float tx,float ty){
  // battle list menus sit on a fixed 66px grid centred at y=325 (720p), in a column at the screen edge
  u64 h=hud(); if(!h) return 0;
  if(!((tx>=940.f&&tx<=1265.f)||(tx>=15.f&&tx<=340.f))) return 0;
  u64 cmd=h+0x1068, map=h+0xab8;
  if(widget_vis(cmd)) return list_tap(cmd,325.f,ty,"cmd");
  if(widget_vis(map)) return list_tap(map,325.f,ty,"map");
  return 0;
}
// ---- touch gesture tracking ----
static int t_down, t_settle; static float t_x0,t_y0,t_x,t_y; static u64 t_t0; static int t_long, t_swiped; static float t_sx,t_sy;
static u64 tick_freq=19200000; // Switch CNTFRQ
// global common-UI object (message boxes etc.)
static u64 cmn_ui(void){ if(!g_main) return 0; u64 p=RD64(g_main+0x21d9ae0); return (p&&mem_ok(p))?p:0; }
// "wheel" style list: highlighted row stays at a fixed y and the list scrolls around it
static int wheel_tap(u64 w,float ty,float hy,float sp,const char*name){
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 0;
  float f=(ty-hy)/sp; int d=(int)(f+(f>=0?0.5f:-0.5f)); int it=idx+d; if(it<0||it>=cnt) return 1;
  list_jump(w,it,1);
  if(tlog){ Buf b={0}; bstr(&b,"[touchmod] "); bstr(&b,name); bstr(&b," item "); bint(&b,it); bflush(&b); }
  return 1;
}
// The common UI object G also owns the pointing hand used by most full-screen menus:
// G+0x50 = hand active, G+0x58 = widget that owns it, G+0x2c/+0x30 = hand x,y (1080p).
static int rows_tap(u64 w,float x,float y,float x0,float x1,float y0,float sp,const char*name);
static void tm_log2(const char*m,s64 a,s64 b);
#define G_HAND_ON 0x50
#define G_HAND_OWNER 0x58
static int hand_on(u64 g){ return *(volatile u8*)(g+G_HAND_ON)!=0; }
// title flow screens: difficulty / mode choice (centred rows 100px apart) and the save file list
static int title_flow_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0;
  u64 o=RD64(g+G_HAND_OWNER), w=0;
  if(o==g+0xa0d40 && RD64(o)==g_main+0x1a4d710) w=o;        // difficulty
  else if(o==g+0xa1360 && RD64(o)==g_main+0x1a4da80) w=o;   // Casual / Classic
  if(w){ s32 cnt=RDS32(w+0x54); if(cnt<=0||cnt>6) return 1;
    float y0=338.f-100.5f*(float)(cnt-1)*0.5f; rows_tap(w,x,y,300.f,975.f,y0,100.5f,"choice"); return 1; }
  u64 sl=g+0x60770; float hx=RDF(g+0x2c);
  u64 own=RD64(g+G_HAND_OWNER); u64 ovt= mem_ok(own) ? RD64(own)-g_main : 0;
  // the list widget's vis byte stays set after a file is loaded, so also require the hand to belong to the file list
  int load_list = RD64(sl)==g_main+0x1a3c1f8 && widget_vis(sl) && ovt==0x1a3aee0;
  int save_list = !load_list && hx>=225.f && hx<=237.f    // save mode uses another widget; recognise it by the hand column
                  && ovt!=0x1a431f8 && own!=g+0x2250;      // ...but not the Options list, whose hand passes through that column
  if(load_list || save_list){                              // save file list (Continue / Load / Save)
    s32 tab= load_list ? RDS32(g+0x60a98) : -1;
    if(y<95.f){ // Main Story / Side Story tabs (ZL / ZR)
      if(x>=720.f&&x<950.f&&tab==1) q_push(1ul<<8,2,4);
      else if(x>=950.f&&x<=1170.f&&tab==0) q_push(1ul<<9,2,4);
      return 1; }
    if(x<100.f||x>1150.f||y>655.f) return 0;
    float hy=RDF(g+0x30)/1.5f-5.f; float f=(y-hy)/140.f; int d=(int)(f+(f>=0?0.5f:-0.5f));
    if(ovt==0x1a3aee0){ s32 idx=RDS32(own+0x4c), cnt=RDS32(own+0x54); if(idx+d<0||idx+d>=cnt) return 1; list_jump(own,idx+d,1); }
    else { u64 b=d>0?BTN_D:BTN_U; int n=d<0?-d:d; for(int i=0;i<n;i++) q_push(b,1,3); q_push(BTN_A,2,3); }
    tm_log2("save slot",d,(s64)hy); return 1; }
  return 0;
}
static int title_menu_tap(float x,float y){
  u64 g=cmn_ui(); if(!g) return 0; u64 w=g+0xa0760;
  if(RD64(w)!=g_main+0x1a3f628 || !widget_vis(w) || hand_on(g)) return 0;
  if(x<900.f) return 1; // menu is on the right; ignore taps elsewhere
  return wheel_tap(w,y,279.f,80.f,"title");
}
static int yesno_tap(float x,float y){
  u64 g=cmn_ui(); if(!g) return 0; u64 w=g+0xe00;
  if(!widget_vis(w)) return 0;
  s32 idx=RDS32(g+0xe50), cnt=RDS32(g+0xe58); if(cnt!=2||idx<0||idx>1) return 0;
  if(y<370.f||y>480.f||x<370.f||x>910.f) return 1; // dialog open: ignore taps outside its buttons
  int want = x<640.f ? 0 : 1;
  if(want!=idx) q_push(want==0?BTN_L:BTN_R,1,2);
  q_push(BTN_A,2,3);
  if(tlog){ Buf b={0}; bstr(&b,"[touchmod] yesno "); bint(&b,want); bflush(&b); }
  return 1;
}
// vertical list with fixed row geometry: row i centre = y0 + sp*i
static int rows_tap(u64 w,float x,float y,float x0,float x1,float y0,float sp,const char*name){
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 0;
  if(x<x0||x>x1) return 0;
  float f=(y-y0)/sp; int it=(int)(f+0.5f+100.f)-100; if(it<0||it>=cnt) return 0;
  list_jump(w,it,1);
  if(tlog){ Buf b={0}; bstr(&b,"[touchmod] "); bstr(&b,name); bstr(&b," row "); bint(&b,it); bstr(&b," from "); bint(&b,idx); bflush(&b); }
  return 1;
}
static int rows_pick(u64 w,float x,float y,float x0,float x1,float y0,float sp,const char*name){   // rows_tap with double tap to confirm
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 0;
  if(x<x0||x>x1) return 0;
  float f=(y-y0)/sp; int it=(int)(f+0.5f+100.f)-100; if(it<0||it>=cnt) return 0;
  list_pick(w,it); tm_log2(name,it,idx); return 1; }
static int inbox(float x,float y,float x0,float x1,float y0,float y1){ return x>=x0&&x<=x1&&y>=y0&&y<=y1; }
static int is_widget(u64 w,u64 vt){ return mem_ok(w) && RD64(w)==g_main+vt && widget_vis(w); }
// deferred re-tap: some taps first have to move focus (e.g. between the two convoy panels);
// the tap is then replayed once the queued presses are done and the screen has settled
static int rt_n, rt_depth; static float rt_x, rt_y;
static void on_tap(float x,float y);
static u32 rt_probe_vt; static int rt_probe_x;   // a probe step is waiting for the pitch of this list class (x: column pitch)
static int rt_wait_det;   // replay as soon as the status sheet's Details mode is up (instead of after a fixed delay)
static int status_det_now(void); static int bdet_det_now(void);
static int rt_det_now(void){ return rt_wait_det==2 ? bdet_det_now() : status_det_now(); }   // 1 status sheet, 2 battle unit details
static u64 rt_own;   // replay as soon as the hand owner is no longer this one (a popup or the next screen took the hand)
static int rt_settle, rt_still; static float rt_hx, rt_hy;   // with rt_own: also wait until the hand has stopped moving
static void retap_later(float x,float y,int polls){ if(rt_depth>=2) return; rt_x=x; rt_y=y; rt_n=polls; rt_probe_vt=0; rt_wait_det=0; rt_own=0; rt_settle=0; rt_still=0; }
static void retap_on_change(float x,float y,int polls){ u64 g=cmn_ui(); retap_later(x,y,polls); if(g) rt_own=RD64(g+0x58); }
static float learned_pitch(u32 vt); static float learned_xpitch(u32 vt);
static void rt_tick(void){ if(rt_n<=0) return;
  // status sheet: replay the tap on the very poll Details mode comes up (the rest of the X press gap is not needed)
  if(rt_wait_det && q_phase!=1 && qh==qt && rt_det_now()){ q_clear(); rt_n=1; }
  else if(q_busy()) return;
  // probe: replay the tap as soon as the step has taught us the pitch, instead of waiting out the full delay
  if(rt_probe_vt && (rt_probe_x ? learned_xpitch(rt_probe_vt) : learned_pitch(rt_probe_vt))>0.f) rt_n=1;
  if(rt_wait_det){ if(rt_det_now()) rt_n=1; else if(rt_n==1){ rt_n=0; rt_wait_det=0; return; } }
  if(rt_own){ u64 g=cmn_ui(); if(g && *(volatile u8*)(g+0x50) && RD64(g+0x58)!=rt_own && RD64(g+0x58)){
      if(!rt_settle) rt_n=1;
      else { float hx=RDF(g+0x2c), hy=RDF(g+0x30); if(hx==rt_hx && hy==rt_hy){ if(++rt_still>=2) rt_n=1; } else { rt_still=0; rt_hx=hx; rt_hy=hy; } } } }
  if(--rt_n==0){ rt_probe_vt=0; rt_wait_det=0; rt_own=0; rt_depth++; on_tap(rt_x,rt_y); rt_depth--; } }
static void probe_later(float x,float y,u32 vt,int xdir){ retap_later(x,y,30); rt_probe_vt=vt; rt_probe_x=xdir; }
// ---- hand chase: walk the pointing hand to a tapped spot with real d-pad presses ----
// For screens whose cursor moves between many separate lists (the status sheet in Details mode): the game's own navigation
// keeps help popups and focus right. After each press we wait for the hand (G+0x2c/+0x30) to settle, then pick the next
// direction. The hand sits at the left end of the selected item, vertically centred.
static int hc_on, hc_steps, hc_settle, hc_back, hc_novert; static float hc_tx, hc_ty, hc_lx, hc_ly, hc_px, hc_py, hc_tol=17.f; static u64 hc_dir, hc_arm_w;
static u64 cmn_ui(void);
static void dt_arm(u64 key,s32 val,u64 key2,s32 val2);
static void hand_chase(float x,float y){ u64 g=cmn_ui(); if(!g) return; hc_on=1; hc_tx=x; hc_ty=y; hc_steps=0; hc_settle=0; hc_dir=0; hc_back=0; hc_novert=0; hc_tol=17.f; hc_arm_w=0;
  hc_lx=hc_px=RDF(g+0x2c)/1.5f; hc_ly=hc_py=RDF(g+0x30)/1.5f; }
static void hc_tick(void){
  if(!hc_on){ if(hc_arm_w && !q_busy()){ dt_arm(hc_arm_w+0x4c,RDS32(hc_arm_w+0x4c),0,0); hc_arm_w=0; } return; }   // the row reached is armed: the next tap on it confirms
  if(q_busy()) return; u64 g=cmn_ui(); if(!g || !*(volatile u8*)(g+0x50)){ hc_on=0; return; }
  float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f;
  if(hx!=hc_lx || hy!=hc_ly){ hc_lx=hx; hc_ly=hy; hc_settle=0; return; }
  if(++hc_settle<3) return;                                   // hand still for 3 polls
  if(hc_back){ hc_on=0; return; }
  float dy=hc_ty-hy, ady=dy<0?-dy:dy;
  if(hc_dir){                                                  // judge the last step
    int moved = hx!=hc_px || hy!=hc_py;
    float pdy=hc_ty-hc_py; if(pdy<0) pdy=-pdy;
    if(!moved){ if((hc_dir==BTN_U||hc_dir==BTN_D) && !hc_novert) hc_novert=1;   // end of a column: try sideways
      else { hc_on=0; tm_log2("chase stuck",(s64)hx,(s64)hy); return; } }
    if((hc_dir==BTN_U||hc_dir==BTN_D) && ady>pdy+4.f){ q_push(hc_dir==BTN_U?BTN_D:BTN_U,2,3); hc_back=1; return; } // overshot: back
    if(hc_dir==BTN_R && hx>hc_tx+15.f){ q_push(BTN_L,2,3); hc_back=1; return; }   // the next item starts right of the tap
  }
  if(++hc_steps>16){ hc_on=0; return; }
  hc_px=hx; hc_py=hy; u64 d=0;
  if(ady>hc_tol && !hc_novert) d= dy<0 ? BTN_U : BTN_D;
  else if(hc_tx<hx-25.f) d=BTN_L;
  else if(hc_tx>hx+165.f) d=BTN_R;
  else if(ady>hc_tol && hc_novert==1){ hc_novert=2; d= dy<0 ? BTN_U : BTN_D; }
  if(d==BTN_L||d==BTN_R) hc_novert=0;                       // a new column: vertical steps may work again
  if(!d){ hc_on=0; tm_log2("chase done",(s64)hx,(s64)hy); return; }
  hc_dir=d; hc_settle=0; q_push(d,2,3); }
static void tm_log2(const char*m,s64 a,s64 b){ if(!tlog) return; Buf bb={0}; bstr(&bb,"[touchmod] "); bstr(&bb,m); bput(&bb,' '); bint(&bb,a); bput(&bb,' '); bint(&bb,b); bflush(&bb); }
// convoy screen (G = common UI object)
#define CV_UL   0xd238   // unit item list widget, vtable main+0x1a4a218
#define CV_US   0xdf80   // Store/Trade submenu,   vtable main+0x1a42478
#define CV_TS   0xe598   // Take/Trade submenu,    vtable main+0x1a42808
#define CV_HAND 0xebb0   // pointing-hand cursor,  vtable main+0x19edb28; +0x2c/+0x30 = x,y (1080p)
#define CV_CNT  0x111d0  // rows in the current convoy tab
#define CV_TAB  0x111ec  // current convoy tab (0..6)
#define CV_FOC  0x111f0  // 1 = convoy list has focus, 0 = unit list
#define CV_IDX  0x111e8  // selected convoy row (absolute index)
static float cv_row_y(u64 g){ // 720p centre of the currently highlighted convoy row, from the hand cursor
  u64 hd=g+CV_HAND; if(RD64(hd)!=g_main+0x19edb28) return -1.f; return RDF(hd+0x30)/1.5f-21.4f; }
static int cv_top_ok, cv_still; static s32 cv_top, cv_li; static float cv_ly;
static void cv_tick(void){ u64 g=cmn_ui(); if(!g || !is_widget(g+CV_UL,0x1a4a218) || !RDS32(g+CV_FOC)){ cv_top_ok=0; cv_still=0; return; }
  float hy=cv_row_y(g); if(hy<0.f){ cv_top_ok=0; return; }
  s32 ci=RDS32(g+CV_IDX);
  if(hy!=cv_ly || ci!=cv_li){ cv_ly=hy; cv_li=ci; cv_still=0; return; }   // index change: the hand starts moving a frame later
  if(++cv_still<10 || q_busy()) return;
  float fr=(hy-232.f)/35.2f; int r=(int)(fr+0.5f); float e=fr-(float)r; if(e<0) e=-e; if(e>0.12f) return;   // hand exactly on a row
  s32 cnt=RDS32(g+CV_CNT); int t=ci-r, tmax= cnt>11 ? cnt-11 : 0; if(t<0) t=0; if(t>tmax) t=tmax;           // 11 rows visible
  cv_top=t; cv_top_ok=1; }
static int convoy_tap(u64 g,float x,float y){
  u64 ul=g+CV_UL, us=g+CV_US, ts=g+CV_TS;
  if(!is_widget(ul,0x1a4a218)) return 0;
  // Store / Trade and Take / Trade submenus: rows 33.6 apart. The game opens them below the row, or above it near the bottom of the
  // list (the last convoy rows), so the rows are placed from the hand on the submenu (3 px above a row's middle) when it has it.
  #define CV_SUB_Y0(w,dflt) ((hand_on(g) && RD64(g+G_HAND_OWNER)==(w)) ? RDF(g+0x30)/1.5f+3.f-33.6f*(float)RDS32((w)+0x4c) : (dflt))
  if(is_widget(us,0x1a42478)){ float y0=CV_SUB_Y0(us,286.6f+35.2f*(float)RDS32(ul+0x4c));
    rows_tap(us,x,y,490.f,645.f,y0,33.6f,"store"); return 1; }
  if(is_widget(ts,0x1a42808)){ float ry=cv_row_y(g); float y0=CV_SUB_Y0(ts, ry>0.f ? ry-12.f : 220.f);
    rows_tap(ts,x,y,480.f,630.f,y0,33.6f,"take"); return 1; }
  int foc=RDS32(g+CV_FOC)!=0, tab=RDS32(g+CV_TAB); if(tab<0||tab>6) return 1;
  // category tabs above the convoy list
  if(x>=795.f&&x<=1022.f&&y>=124.f&&y<=166.f){
    int k=(int)((x-812.f)/31.5f+0.5f); if(k<0)k=0; if(k>6)k=6;
    if(!foc){ q_push(BTN_R,1,3); retap_later(x,y,6); return 1; }
    if(k==tab) return 1;
    // instant: put the tab next to the target, one real step makes the game load the target tab
    *(volatile s32*)(g+CV_TAB)= k>tab ? k-1 : k+1; q_push(k>tab?BTN_R:BTN_L,2,3);
    tm_log2("tab",k,tab); return 1; }
  // arrows at both ends of the tab strip, unit switch arrows (L name R) above the unit panel, X Details
  if(y>=122.f&&y<=166.f&&((x>=640.f&&x<705.f)||(x>=1125.f&&x<=1190.f))){
    if(!foc){ q_push(BTN_R,1,3); retap_later(x,y,6); return 1; }
    q_push(x<900.f?BTN_L:BTN_R,2,3); return 1; }
  if(y>=88.f&&y<=128.f&&x>=110.f&&x<=190.f){ q_push(BTN_LB,2,4); return 1; }
  if(y>=88.f&&y<=128.f&&x>=420.f&&x<=500.f){ q_push(BTN_RB,2,4); return 1; }
  if(inbox(x,y,1180.f,1264.f,682.f,718.f)){ q_push(BTN_X,2,4); return 1; }
  // convoy list rows
  if(x>=665.f&&x<=1150.f&&y>=214.f&&y<=650.f){
    if(!foc){ q_push(BTN_R,1,3); retap_later(x,y,6); return 1; }
    s32 cnt=RDS32(g+CV_CNT); float cy=cv_row_y(g); if(cnt<=0||cy<0.f) return 1;
    int rt=(int)((y-214.4f)/35.2f), rc=(int)((cy-214.4f)/35.2f+0.5f); // visible row index (row 0 centre = 232)
    // the hand glides to a new row for a few frames while the index has already changed: use the scroll position seen
    // the last time the hand was at rest (cv_tick), so quick taps don't land a row or two off
    if(cv_top_ok) rc=RDS32(g+CV_IDX)-cv_top;
    else if(cnt<=11) rc=RDS32(g+CV_IDX);   // short list: never scrolled
    if(cnt<=11 && rt>=cnt) return 1;   // short list: ignore taps below the last row
    // instant: the selected row index is G+0x111e8 (the row record G+0x111c0 is looked up from it on each step), so put
    // the index next to the target and take one real step
    int d=rt-rc; s32 ci=RDS32(g+CV_IDX); int t=ci+d; if(t<0||t>=cnt) return 1;
    int conf=dt_same(g+CV_IDX,t,0,0) || (!d && !q_busy());   // the selected row confirms at once
    if(d){ int pre= d>0 ? t-1 : t+1; if(pre!=ci) *(volatile s32*)(g+CV_IDX)=pre; g_learn_hold=20; q_push(d>0?BTN_D:BTN_U,2,3); }
    if(conf){ dt_on=0; q_push(BTN_A,2,3); } else dt_arm(g+CV_IDX,t,0,0);   // double tap to confirm
    tm_log2("convoy row",rt,rc); return 1; }
  // unit item list
  if(x>=110.f&&x<=490.f&&y>=268.f&&y<=500.f){
    // back to the unit list: Left on the first tab moves the focus there (on other tabs Left changes tab). Instant: the tab next to
    // the first one, one real step loads the first tab, one more moves the focus (walking every tab flipped through all of them)
    if(foc){ if(tab>1) *(volatile s32*)(g+CV_TAB)=1; if(tab>0) q_push(BTN_L,2,3); q_push(BTN_L,1,3); retap_later(x,y,6); return 1; }
    rows_pick(ul,x,y,110.f,490.f,286.6f,35.2f,"unit items"); return 1; }
  return 0; // let the generic handlers (e.g. bottom-right Back) have it
}
// Options screen (map menu -> Options): common UI list at G+0x2250, tabs Battle/Monastery/General
#define OP_LIST 0x2250   // vtable main+0x1a431f8; +0x4c idx, +0x54 cnt, u16 at +0x6a = first visible row
#define OP_TAB  0x2bd0   // 0 Battle, 1 Monastery, 2 General (ZL/ZR)
static int options_tap(u64 g,float x,float y){
  u64 w=g+OP_LIST; if(!is_widget(w,0x1a431f8)) return 0;
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54), tab=RDS32(g+OP_TAB); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 1;
  if(inbox(x,y,1100.f,1264.f,682.f,718.f)){ q_push(BTN_MINUS,2,4); return 1; }   // "- Restore defaults"
  if(y>=108.f&&y<=158.f){ // tab strip
    int k=-1; if(x>=290.f&&x<=380.f) k=tab-1; else if(x>=900.f&&x<=1000.f) k=tab+1; else if(x>=455.f&&x<=820.f) k=(int)((x-519.f)/120.f+0.5f);
    if(k<0||k>2||(tab<0||tab>2)) return 1;
    if(k==tab) return 1;
    // instant: put the tab next to the target, one real ZL/ZR step loads it (as with the convoy tabs)
    *(volatile s32*)(g+OP_TAB)= k>tab ? k-1 : k+1; q_push(k>tab?BTN_ZR:BTN_ZL,2,4);
    tm_log2("options tab",k,tab); return 1; }
  if(x>=300.f&&x<=975.f&&y>=185.f&&y<=556.f){
    int top=(int)((RD32(w+0x68)>>16)&0xffff); if(top<0||top>=cnt) top=0;
    int r=(int)((y-185.1f)/34.8f); int it=top+r; if(it<0||it>=cnt) return 1;
    list_jump(w,it,0);
    if(x>=655.f&&x<=735.f) q_push(BTN_L,2,3);        // left arrow
    else if(x>=770.f) q_push(BTN_R,2,3);              // value or right arrow
    tm_log2("options row",it,idx); return 1; }
  return 0;
}
// item screens: battle "Items" list + its Use/Trade/Discard submenu, and the convoy screen.
// returns 1 if the tap was consumed
static int items_tap(float x,float y){
  u64 h=hud();
  if(h){
    u64 lst=h+0xee20, sub=h+0x1c630;
    if(is_widget(lst,0x19f9678)){
      if(is_widget(sub,0x19f9830)){
        float y0=131.f+35.6f*(float)RDS32(lst+0x4c)-2.5f;
        rows_tap(sub,x,y,660.f,830.f,y0,34.5f,"itemsub");
        return 1; }
      rows_pick(lst,x,y,835.f,1215.f,131.f,35.6f,"items");
      return 1; }
  }
  u64 g=cmn_ui();
  if(g && options_tap(g,x,y)) return 1;
  if(g && convoy_tap(g,x,y)) return 1;
  return 0;
}
// ---- generic list support ----
// Every list menu in the game derives from one cursor base class (index +0x4c, count +0x54, first visible
// row s16 +0x6a). Its vtable slots 26/27 are always main+0x5de4e0 / main+0x5de4f0, so any list widget can be
// recognised without knowing which menu it is.
static int is_list(u64 w){
  if(!mem_ok(w)) return 0; u64 vt=RD64(w); if(vt<g_main+0x19e4000||vt>=g_main+0x1ab6000) return 0;
  u64 s27=RD64(vt+27*8);   // 2-D grids (unit pickers) override slot 27 with main+0x5eca90
  return RD64(vt+26*8)==g_main+0x5de4e0 && (s27==g_main+0x5de4f0 || s27==g_main+0x5eca90); }
static int list_row(u64 w){ return RDS32(w+0x4c)-(s32)(short)(RD32(w+0x68)>>16); } // visible row of the selection
// Row pitch learned at runtime. The selection indicator (the pointing hand, or a highlight) moves to the
// selected row; two settled indicator positions at different visible rows give the pitch of that list class.
typedef struct { u32 vt; float pitch, xpitch; } Learn;   // row pitch (y) and column pitch (x), 720p
static Learn lrn[32]; static int nlrn;
static Learn* lrn_get(u32 vt,int make){ for(int i=0;i<nlrn;i++) if(lrn[i].vt==vt) return &lrn[i];
  if(!make||nlrn>=32) return 0; lrn[nlrn].vt=vt; lrn[nlrn].pitch=0.f; lrn[nlrn].xpitch=0.f; return &lrn[nlrn++]; }
// measured pitches for lists we know, so the first tap on them needs no probe step
typedef struct { u32 vt; float pitch, xpitch, yoff; } Seed;   // yoff: row middle minus hand y (720p), for lists whose hand sits above the row's middle
static const Seed seeds[]={
  {0x1a44878, 53.3f, 380.f},   // Inventory unit picker (2 columns)
  {0x1a44138, 53.6f, 382.7f},  // Support: unit grid (2 columns)
  {0x1a20ce8, 53.3f, 0.f},     // Unit Appearance: unit list
  {0x1a1ea90, 53.6f, 0.f},     // Reclass: unit list
  {0x1a1ec48, 100.f, 100.7f},  // Reclass: class grid (guess from the status Classes page)
  {0x1a1ee00, 53.33f, 0.f},    // Reclass / Goals: class list
  {0x1a1b850, 53.33f, 0.f},    // Cooking Together: unit list
  {0x1a1e048, 53.33f, 0.f},    // Instruct: unit list
  {0x1a1ddc8, 53.33f, 0.f},    // Instruct: topics (skills)
  {0x1a1db48, 111.3f, 0.f},    // lecture: answers to a student's question
  {0x1a0eaf0, 148.2f, 0.f},    // lecture: Group Tasks
  {0x1a1f768, 53.33f, 0.f},    // Group Tasks: unit list
  {0x1a1ba18, 34.67f, 0.f},    // Cooking Together: recipes
  {0x1a221c0, 0.f, 281.3f},    // quest Accept / Decline
  {0x1a43a68, 53.3f, 0.f},     // Support: support levels list
  {0x1a07bd8, 67.2f, 0.f},     // calendar menu
  {0x1a20eb0, 34.67f, 0.f},    // Unit Appearance: options popup (Attire / In Battle)
  {0x1a21070, 34.67f, 0.f},    // Unit Appearance: attire / in-battle lists
  {0x1a24d78, 53.33f, 0.f},    // Goals: unit list
  {0x1a24f40, 34.67f, 0.f},    // Goals: focus list
  {0x1a250f8, 34.67f, 0.f},    // Goals: custom focus skills
  {0x1a083f0, 75.3f, 0.f},     // Gardening: Plant / Cultivate / Harvest (also world prompts, count 1)
  {0x1a1d1b8, 34.67f, 0.f},    // Gardening: cultivation methods
  {0x1a1cd88, 34.67f, 0.f},    // Gardening: seeds (amount picker on each row)
  {0x1a4e660, 34.67f, 0.f},     // Extras: Movie Gallery
  {0x1a4ddf0, 34.67f, 0.f},     // Extras: Music Library
  {0x1a1b0b0, 53.33f, 0.f},    // Dining hall: Today's Special unit list
  {0x1a25620, 35.0f, 0.f},     // DLC Supplies: sets
  {0x19f0e48, 33.3f, 0.f},     // Battle prep wheel: System -> Options / Guide
  {0x19ee290, 34.6f, 0.f},     // Marketplace: Armory / Item Shop / Battalion Guild / Blacksmith
  {0x1a47be8, 53.3f, 0.f},     // shops: unit list
  {0x1a47f78, 34.65f, 0.f},    // shops: Buy / Sell item list
  {0x1a25c88, 33.3f, 0.f},     // shops: Buy (Sell) / Cancel popup
  {0x1a24bc0, 48.f, 0.f},      // calendar schedule details: events
  {0x1a46d08, 53.33f, 0.f},    // Battalion Guild: Hire unit list
  {0x1a47098, 34.67f, 0.f},    // Battalion Guild: Available for Hire / Dismiss
  {0x1a474e8, 34.7f, 0.f},     // Battalion Guild: Replenish
  {0x1a1a820, 34.67f, 0.f, 12.8f},  // Lost Items (talk to a unit): the hand sits 12.8 px above a row's middle
  {0x1a1ab88, 34.67f, 0.f, 12.8f},  // Tea party: tea leaves (same layout as Lost Items)
  {0x1a1d528, 0.f, 360.f},          // Tea party: the three conversation topics (1x3 button row, timed)
  {0x1a1e2d8, 0.f, 261.3f},         // lecture: "Update goals?" Yes / No (1x2 button row)
  {0x1a1a4d0, 53.33f, 0.f},         // monastery map -> X Locate: People / Facilities list
  {0x1a22378, 34.67f, 0.f, 3.5f},   // pause menu -> Quests: quest list (hand 3.5 px above a row's middle)
  {0x1a483c8, 33.3f, 0.f, 3.2f},    // Storehouse: Sell / Cancel popup
  {0x1a44a48, 34.67f, 0.f, 11.8f},  // calendar -> Battle: Battlefield mission list (hand 11.8 px above a row's middle)
  {0x1a1e648, 53.33f, 0.f, -3.7f},  // calendar -> Seminar: lecturer list (hand 3.7 px below a row's middle)
  {0x19ed970, 34.67f, 0.f, -2.5f},  // battle map menu -> Auto-Battle: Charge / Focus / Fall Back / Unite (hand 2.5 px below a row's middle)
  {0x1a1c878, 53.33f, 0.f},         // Tournament organizer: unit list
  {0x1a48aa8, 34.6f, 0.f, 14.f},    // pause menu -> Storehouse: item list (hand 14 px above a row's middle)
};
static const Seed* seed_get(u32 vt){ for(unsigned i=0;i<sizeof(seeds)/sizeof(seeds[0]);i++) if(seeds[i].vt==vt) return &seeds[i]; return 0; }
static float learned_pitch(u32 vt){ Learn*l=lrn_get(vt,0); if(l&&l->pitch>0.f) return l->pitch; const Seed*d=seed_get(vt); return d?d->pitch:0.f; }
static float learned_xpitch(u32 vt){ Learn*l=lrn_get(vt,0); if(l&&l->xpitch>0.f) return l->xpitch; const Seed*d=seed_get(vt); return d?d->xpitch:0.f; }
static void set_pitch(u32 vt,float p){ Learn*l=lrn_get(vt,1); if(l) l->pitch=p; }
static void set_xpitch(u32 vt,float p){ Learn*l=lrn_get(vt,1); if(l) l->xpitch=p; }
static int list_col(u64 w){ return RDS32(w+0x50)-(s32)(short)(RD32(w+0x68)&0xffff); } // visible column of the selection
static u64 li_w; static int li_row, li_col, li_stable, li_hrow, li_hcol; static float li_x, li_y, li_hx, li_hy;
// Where visible row 0 of the active list sits (hand y, 720p), taken whenever the hand has settled. A tap that arrives while the hand
// is still gliding to a newly selected row (e.g. the second tap of a quick double tap) is placed with it instead of the moving hand,
// which put it a row off (it selected the next row instead of confirming). Kept only while that list stays the active one.
static u64 g_r0_w; static float g_r0_y;
static void learn_feed(u64 w,float ix,float iy){ // once per poll: the active list and its indicator x,y (720p), or w=0
  if(w!=g_r0_w) g_r0_w=0;
  if(!w){ li_w=0; return; }
  if(g_learn_hold>0){ g_learn_hold--; li_w=0; return; }   // restart from a fresh baseline afterwards
  int row=list_row(w), col=list_col(w);
  if(w!=li_w){ li_w=w; li_hrow=-99999; li_stable=0; li_row=row; li_col=col; li_x=ix; li_y=iy; return; }
  if(row!=li_row || col!=li_col || iy!=li_y || ix!=li_x){ li_row=row; li_col=col; li_x=ix; li_y=iy; li_stable=0; return; }
  if(++li_stable!=4) return;           // indicator settled for 4 polls
  u32 vt=(u32)(RD64(w)-g_main);
  { float p=learned_pitch(vt); if(p>0.f && RDS32(w+0x58)<=1){ g_r0_w=w; g_r0_y=iy-(float)row*p; } }   // 1-D lists only
  if(li_hrow!=-99999 && row!=li_hrow && col==li_hcol){ float p=(iy-li_hy)/(float)(row-li_hrow);
    if(p>15.f&&p<200.f){ set_pitch(vt,p); tm_log2("learned pitch x10",(s64)(p*10.f),(s64)vt); } }
  if(li_hrow!=-99999 && col!=li_hcol && row==li_hrow){ float p=(ix-li_hx)/(float)(col-li_hcol);
    if(p>15.f&&p<700.f){ set_xpitch(vt,p); tm_log2("learned xpitch x10",(s64)(p*10.f),(s64)vt); } }
  li_hrow=row; li_hcol=col; li_hx=ix; li_hy=iy;
}
// Lists without the pointing hand: the selected row is marked by a highlight whose target position is known.
// {list widget offset in G, its vtable, offset in G of the highlight's target x,y (1080p), default pitch, x range}
typedef struct { u32 off, vt, ind; float pitch, x0, x1; const char*name; } HlList;
static const HlList hl_lists[]={
  {0xa1980, 0x1a3ed18, 0xa21e8, 60.f, 330.f, 950.f, "extras"},   // title menu -> Extras (Music Library, Movie Gallery, ...)
};
static const HlList* hl_active(u64 g){
  if(hand_on(g)) return 0;
  for(unsigned i=0;i<sizeof(hl_lists)/sizeof(hl_lists[0]);i++){ u64 w=g+hl_lists[i].off; if(is_widget(w,hl_lists[i].vt)) return &hl_lists[i]; }
  return 0; }
static int hl_list_tap(u64 g,float x,float y){
  const HlList*h=hl_active(g); if(!h) return 0; u64 w=g+h->off;
  s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54); if(cnt<=0||cnt>60||idx<0||idx>=cnt) return 1;
  if(x<h->x0||x>h->x1) return 1;
  float hy=RDF(g+h->ind+4)/1.5f, p=learned_pitch(h->vt); if(p<=0.f) p=h->pitch;
  float f=(y-hy)/p; int d=(int)(f+(f>=0?0.5f:-0.5f)); if(idx+d<0||idx+d>=cnt) return 1;
  list_jump(w,idx+d,1); tm_log2(h->name,d,(s64)hy); return 1; }
static void learn_tick(void){
  u64 g=cmn_ui(); if(!g){ learn_feed(0,0,0); return; }
  if(hand_on(g)){ u64 o=RD64(g+0x58); if(is_list(o) && widget_vis(o)){ learn_feed(o,RDF(g+0x2c)/1.5f,RDF(g+0x30)/1.5f); return; } }
  else { const HlList*h=hl_active(g); if(h){ learn_feed(g+h->off,RDF(g+h->ind)/1.5f,RDF(g+h->ind+4)/1.5f); return; } }
  learn_feed(0,0,0); }
// Left/Right pickers (e.g. "Select a form."): the help bar is up but no hand and no list took the tap.
// Taps on the left or right side send Left / Right (the arrows sit there); the middle falls through to A.
static int picker_tap(u64 g,float x,float y){
  if(hand_on(g) || !is_widget(g+0x2cf8,0x1a43568)) return 0;
  if(y>650.f) return 0;
  if(x<400.f){ q_push(BTN_L,2,3); tm_log2("picker",-1,(s64)x); return 1; }
  if(x>880.f){ q_push(BTN_R,2,3); tm_log2("picker",1,(s64)x); return 1; }
  return 0; }
// ---- menus driven through the common hand pointer (monastery pause menu and friends) ----
// Each profile is keyed by the vtable of the widget that owns the hand (G+0x58).
// Rows are located relative to the hand: row centre = hand_y720 - off, rows `pitch` apart.
typedef struct { u32 vt; float off, pitch, x0, x1; u8 has_idx, out_b; const char*name; } HandProf;
static const HandProf hprof[]={
  {0x1a48738, 16.5f, 39.85f, -40.f, 300.f, 1, 1, "pause"},    // Menu book: Goals ... Calendar (wraps, disabled rows selectable)
  {0x1a40148, -4.7f, 33.4f,  -25.f, 170.f, 1, 1, "popup"},    // small popup submenus (Inventory: Items/Abilities/..., unit: Items/Trade/Store All)
  {0x1a07bd8,  1.2f, 67.2f,  -20.f, 280.f, 1, 1, "calendar"}, // calendar: Explore / Seminar / Battle / Rest
  {0x1a21af0,-22.f, 46.67f, -25.f, 375.f, 1, 1, "bulletin"}, // bulletin board: quests / announcements (section headers take a row)
  {0x1a2de70,  4.0f, 112.7f, -30.f, 720.f, 1, 1, "choice"},
  {0x1a436f8,-11.4f, 53.6f,  -15.f,1075.f, 1, 1, "units"},   // battle map menu -> Units list (G+0x79d8)  // dialogue choices (G+0xbcbc4, e.g. "I'm a ghost/demon/mortal")
  {0x1a3fdc8, -1.9f, 34.7f,  -20.f, 310.f, 1, 1, "guide"},    // Guide: categories
  {0x1a3ff88, -1.9f, 34.7f,  -20.f, 310.f, 1, 1, "guide2"},   // Guide: topics
};
// Abilities / Combat Arts screens (Inventory): the hand owner (vtable 0x1a40300 / 0x1a40498) holds the left-hand list
// at +0x758 (vtable 0x1a42108, a normal list: idx +0x4c, cnt +0x54), so rows are selected instantly with list_jump.
// Abilities has 9 rows in sections (Personal 1, Class 3, Equipped 5) with gaps between the sections; Combat Arts has
// evenly spaced rows. Row centre = hand y; offsets measured from the hand's y on each row.
static const float abil_y[9]={0.f,53.4f,88.f,122.7f,176.7f,211.4f,246.f,280.7f,315.4f};
// The right-hand "Available" list is at owner+0xdb0 (vtable 0x1a41400): rows 35.05 apart, row centre = hand y - 1,
// spanning x 723-1093 (hand x 735) with the first row at y 161.6. A on an equipped slot moves the hand there.
static int abilities_tap(u64 g,u64 o,u64 vt,float x,float y){
  if(vt!=0x1a40300 && vt!=0x1a40498 && vt!=0x1a40630) return 0;
  int batt= vt==0x1a40630;   // Battalions: same layout, the right list is the Barracks (vt 0x1a41770, wider rows), L/R unit arrows, X Sort
  if(batt){ if(y>=88.f&&y<=128.f){ if(x>=35.f&&x<=120.f){ press(BTN_LB); return 1; } if(x>=380.f&&x<=470.f){ press(BTN_RB); return 1; } }
    if(inbox(x,y,960.f,1250.f,42.f,82.f)){ press(BTN_X); return 1; } }
  u64 l=o+0x758, r=o+0xdb0; if(!is_widget(l,0x1a42108)) return 1;
  int abil= vt==0x1a40300;
  s32 idx=RDS32(l+0x4c), cnt=RDS32(l+0x54); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 1;
  int rok=is_widget(r,batt?0x1a41770:0x1a41400); s32 ridx= rok? RDS32(r+0x4c) : -1, rcnt= rok? RDS32(r+0x54) : 0;
  float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f;
  if(hx>400.f){                                  // the hand is in the Available list
    if(x<560.f){ if(rt_depth==0){ q_push(BTN_B,2,3); retap_later(x,y,20); } return 1; } // tap on the left list: back there first
    if(!rok || rcnt<=0 || rcnt>200 || ridx<0 || ridx>=rcnt) return 1;
    if(x<hx-15.f || x>hx+(batt?680.f:360.f)) return 1;
    float f=(y-(hy-1.f))/35.05f; int d=(int)(f+(f>=0?0.5f:-0.5f)); int it=ridx+d;
    if(it<0||it>=rcnt) return 1;
    list_pick(r,it);   // double tap to assign (the selected row: one tap)
    tm_log2(abil?"avail abilities":"avail arts",it,ridx); return 1; }
  // Taps on the Available list while the left list has the hand are ignored: A on a filled slot opens
  // Unequip / Trade instead of the list, so pick the slot first (an empty slot moves the hand to the list).
  if(x<hx-25.f || x>hx+360.f) return 1;          // rows span x 171-541 with the hand at 187
  int tab= abil && cnt==9;
  float base= tab? abil_y[idx] : 34.65f*(float)idx;
  int best=-1; float bd=1e9f;
  for(int i=0;i<cnt;i++){ float ry=hy+(tab? abil_y[i] : 34.65f*(float)i)-base; float d=y-ry; if(d<0.f) d=-d; if(d<bd){ bd=d; best=i; } }
  if(best<0 || bd>18.f) return 1;                // between sections or outside the list
  list_pick(l,best); tm_log2(abil?"abilities":"arts",best,idx); return 1; }   // double tap (the selected row: one tap)
// command menus among the generic lists: one tap confirms (everything else is an item / unit list: double tap)
static int popup_list(u32 vt){ return vt==0x1a25c88 || vt==0x1a483c8; }   // + Storehouse Sell / Cancel   // small confirm popups (shop Buy / Cancel, ...): one tap, no stop on the rows between
static int single_tap_list(u32 vt){ return popup_list(vt) || vt==0x1a1db48 || vt==0x1a0eaf0 || vt==0x19ee290 || vt==0x1a3be68 || vt==0x1a083f0 || vt==0x1a20eb0 || vt==0x1a07bd8 || vt==0x19f0e48 || vt==0x19ed970; }   // 0x19f0e48: prep wheel System -> Options / Guide
// Recent hand owners. Some screens leave G+0x58 empty after a message box closes (shop: "Sent ... to the convoy") until the
// hand moves; the list under the hand is then the most recent owner that is still a visible list.
static u64 own_hist[4];
static void own_track(void){ u64 g=cmn_ui(); if(!g || !hand_on(g)) return; u64 o=RD64(g+G_HAND_OWNER); if(!o || o==own_hist[0]) return;
  for(int i=3;i>0;i--) own_hist[i]=own_hist[i-1]; own_hist[0]=o; }
static u64 own_fallback(void){ for(int i=0;i<4;i++){ u64 o=own_hist[i]; if(o && is_list(o) && widget_vis(o)) return o; } return 0; }
// Units / Unit Info table (vt 0x1a436f8): Up from the first row puts the hand on the column headers, Left / Right walk the
// columns (crossing into the next page), A sorts by the column (again = the other way round). owner+0x710 = 1 while the hand
// is on the headers, +0x508 = column (numbered across all pages: every page starts with its Name column), +0x50c = page
// (ZL / ZR), +0x510/+0x514 = the hand's spot (1080p). Instant: header mode on, the column next to the target, one real
// Left / Right step (the game moves the hand there), then A. Column hand x (720p) per page, measured:
typedef struct { signed char name, n; float x0, pitch; float xs[5]; } UnitsPage;
static const UnitsPage units_pages[6]={
  { 0,5,0.f,0.f,{434.7f,680.f,776.f,962.7f,1058.7f}},   // Info: Class, Lv, Exp, HP, Max HP
  { 6,9,434.7f,80.f,{0}},                               // 9 columns 80 apart
  {16,2,0.f,0.f,{434.7f,834.7f}},
  {19,8,434.7f,90.f,{0}},
  {28,3,0.f,0.f,{434.7f,694.7f,954.7f}},
  {32,9,434.7f,80.f,{0}},
};
static float units_colx(const UnitsPage*p,int k){ return p->pitch>0.f ? p->x0+p->pitch*(float)k : p->xs[k]; }
static void press(u64 b);
static int units_sort_tap(u64 o,float x,float y){
  int hdr=RDS32(o+0x710)!=0; s32 pg=RDS32(o+0x50c), cc=RDS32(o+0x508);
  if(y>=128.f && y<=170.f){ if(pg<0||pg>5||x<80.f||x>1172.f) return 1;
    const UnitsPage*p=&units_pages[pg]; int t=p->name;            // left of the first data column: Name
    for(int k=0;k<p->n;k++) if(x>=units_colx(p,k)+10.f) t=p->name+1+k;
    if(hdr && cc==t){ press(BTN_A); tm_log2("units sort again",t,pg); return 1; }
    *(volatile s32*)(o+0x710)=1;
    if(t==p->name){ *(volatile s32*)(o+0x508)=t+1; q_push(BTN_L,2,4); } else { *(volatile s32*)(o+0x508)=t-1; q_push(BTN_R,2,4); }
    press(BTN_A); tm_log2("units sort",t,pg); return 1; }
  if(hdr && y>170.f && y<=600.f){ if(rt_depth==0){ press(BTN_D); retap_later(x,y,6); } return 1; }   // a row while the hand is on the headers: back down first
  return 0; }
static int hand_list_tap(u64 g,float x,float y){
  if(!hand_on(g)) return 0;
  u64 o=RD64(g+G_HAND_OWNER); if(!o){ o=own_fallback(); if(o) tm_log2("owner fallback",(s64)(o-g),0); } if(!mem_ok(o)) return 0;
  u64 vt=RD64(o)-g_main; const HandProf*p=0;
  // tea party: "Offer some tea" (vt 0x1a00298) and the "A Talk!" prompt (vt 0x19fff68) are single buttons whose list has no
  // rows (count 0), so the generic list below swallowed every tap: a tap anywhere presses A
  if((vt==0x1a00298 || vt==0x19fff68) && widget_vis(o)){ press(BTN_A); tm_log2("tea button",(s64)vt,0); return 1; }
  // Seminar lecturer list: "ZR Classes" prompt in the corner (the list below swallowed it)
  if(vt==0x1a1e648 && inbox(x,y,1150.f,1264.f,682.f,718.f)){ press(BTN_ZR); tm_log2("seminar classes",0,0); return 1; }
  // Tournament organizer unit list: ZL / ZR arrows on the tournament panel (details / opponents page), right of the list's tap range
  if(vt==0x1a1c878 && y>=275.f && y<=320.f){ if(x>=815.f&&x<=890.f){ press(BTN_ZL); return 1; } if(x>=1180.f&&x<=1260.f){ press(BTN_ZR); return 1; } }
  if(abilities_tap(g,o,vt,x,y)) return 1;
  if(vt==0x1a436f8 && units_sort_tap(o,x,y)) return 1;
  if(vt==0x1a436f8 && y<115.f){ // Units list header: ZL / ZR page arrows, "R Your Units / Enemy Units"
    u64 bb=battle(); int prep= bb && hud() && RDS32(bb+0x3cc0)==29;   // battle prep Unit Info: R there means Back (the "Deploying" box sits in that spot)
    if(x>=470.f&&x<615.f) q_push(1ul<<8,2,4); else if(x>=665.f&&x<=810.f) q_push(1ul<<9,2,4); else if(!prep && x>=940.f&&x<=1180.f) q_push(1ul<<7,2,4);
    return 1; }
  if(vt==0x1a1cd88 && widget_vis(o)){ // Gardening seeds: "< n >" amount picker on each row (arrows at x 372 / 474), A hands them over
    s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); if(cnt<=0||cnt>200||idx<0||idx>=cnt) return 1;
    float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f; if(x<hx-20.f || x>hx+520.f) return 1;
    float f=(y-(hy+12.f))/34.67f; int d=(int)(f+(f>=0?0.5f:-0.5f)); if(idx+d<0||idx+d>=cnt) return 1;
    int arrow= (x>=340.f&&x<=402.f) ? -1 : (x>=444.f&&x<=506.f) ? 1 : 0;
    if(arrow){ list_jump(o,idx+d,0); q_push(arrow<0?BTN_L:BTN_R,2,3); } else list_pick(o,idx+d);
    tm_log2("seeds",idx+d,arrow); return 1; }
  for(unsigned i=0;i<sizeof(hprof)/sizeof(hprof[0]);i++) if(hprof[i].vt==vt){ p=&hprof[i]; break; }
  if(!p){ // any other list that owns the hand: rows located from the hand with the learned pitch
    // battle HUD lists (command / map menu, weapon picker, ...): battle logic. The Auto-Battle popup (vt 0x19ed970, mid-screen) is
    // left to the generic list: the battle menu code only covers the side columns, so its taps did nothing.
    { u64 h=hud(); if(h && o>=h && o<h+0x30000 && vt!=0x19ed970) return 0; }
    if(!is_list(o) || !widget_vis(o)) return 0;
    s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54);
    float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f;
    s32 cidx=RDS32(o+0x50), ccnt=RDS32(o+0x58);
    if(cnt<=1 && ccnt>1 && ccnt<=20 && cidx>=0 && cidx<ccnt){
      // one row of buttons side by side (e.g. Accept / Decline on a quest): columns +0x50 / +0x58, hand x.
      // The hand points at the left part of the selected button, so its centre is about 0.42 pitch to the right.
      if(y<hy-60.f || y>hy+60.f) return 1;
      float xp=learned_xpitch((u32)vt);
      if(xp<=0.f){
        // the selected button itself (the hand points at its left part): confirm at once, no probe step (that step moved the
        // hand to the next button and back before the A: "Yes -> No -> Yes")
        if(x>=hx-30.f && x<=hx+200.f){ q_push(BTN_A,2,3); tm_log2("row selected",cidx,(s64)vt); return 1; }
        if(rt_depth==0){ q_push(cidx+1<ccnt?BTN_R:BTN_L,2,4); probe_later(x,y,(u32)vt,1); tm_log2("probe row",(s64)vt,ccnt); return 1; } xp=280.f; }
      float f=(x-(hx+0.42f*xp))/xp; int d=(int)(f+(f>=0?0.5f:-0.5f)); int it=cidx+d; if(it<0) it=0; if(it>=ccnt) it=ccnt-1; d=it-cidx;
      // instant: the column next to the target (SetColumn), one real step onto it (the game runs its own update), then A
      if(d) grid_jump(o,idx,it,1); else q_push(BTN_A,2,3);
      tm_log2("row",d,(s64)vt); return 1; }
    if(cnt<=0||cnt>200||idx<0||idx>=cnt) return 1;
    if(ccnt>1 && ccnt<=20 && cidx>=0 && cidx<ccnt){
      // 2-D grid (e.g. unit pickers with two columns): rows from the hand y, columns start ~45 px left of the hand
      float yp=learned_pitch((u32)vt), xp=learned_xpitch((u32)vt);
      int on_sel= y>=hy-14.f && y<=hy+14.f && x>=hx-45.f && x<=hx+200.f;   // the selected cell: no probe steps, it confirms
      if(on_sel && (yp<=0.f || xp<=0.f)){ grid_pick(o,idx,cidx); tm_log2("grid selected",idx*100+cidx,(s64)vt); return 1; }
      if(yp<=0.f && cnt>1 && rt_depth<2){ q_push(idx+1<cnt?BTN_D:BTN_U,2,4); probe_later(x,y,(u32)vt,0); tm_log2("probe grid y",(s64)vt,cnt); return 1; }
      if(xp<=0.f && rt_depth<2){ q_push(cidx+1<ccnt?BTN_R:BTN_L,2,4); probe_later(x,y,(u32)vt,1); tm_log2("probe grid x",(s64)vt,ccnt); return 1; }
      if(yp<=0.f) yp=50.f; if(xp<=0.f) xp=380.f;
      float f=(y-hy)/yp; int dr=(int)(f+(f>=0?0.5f:-0.5f));
      float g2=(x-(hx-45.f))/xp; int dc=(int)(g2+100.f)-100;
      int r=idx+dr, c=cidx+dc; if(r<0||r>=cnt||c<0||c>=ccnt) return 1;
      grid_pick(o,r,c); tm_log2("grid",r*100+c,(s64)vt); return 1; }
    if(x<hx-60.f || x>hx+760.f) return 1;
    float pitch=learned_pitch((u32)vt);
    if(pitch<=0.f){
      // unknown list: take one step so the hand shows the row pitch, then replay the tap. Not for a tap on the selected row
      // (the step and the way back made the hand bounce): with the fallback pitch that row comes out as d = 0.
      float sy=hy; { const Seed*sd=seed_get((u32)vt); if(sd) sy+=sd->yoff; }
      int on_sel= y>=sy-14.f && y<=sy+14.f;
      if(!on_sel && cnt>1 && rt_depth==0){ q_push(idx+1<cnt?BTN_D:BTN_U,2,4); probe_later(x,y,(u32)vt,0); tm_log2("probe list",(s64)vt,cnt); return 1; }
      pitch=40.f; }
    float yoff=0.f; { const Seed*sd=seed_get((u32)vt); if(sd) yoff=sd->yoff; } hy+=yoff;
    float f=(y-hy)/pitch; int d=(int)(f+(f>=0?0.5f:-0.5f));
    if(!(li_w==o && li_stable>=4) && g_r0_w==o){   // the hand is still moving: place the tap from the settled row-0 position
      float fr=(y-(g_r0_y+yoff))/pitch; int r=(int)(fr+(fr>=0?0.5f:-0.5f)); int top=(s32)(short)(RD32(o+0x68)>>16);
      d=top+r-idx; tm_log2("list moving",d,(s64)vt); }
    if(idx+d<0||idx+d>=cnt) return 1;
    if(popup_list((u32)vt)){ g_learn_hold=20; if(d) ((SetIdxFn)(g_main+0x5de5b0))(o,idx+d); q_push(0,1,0); press(BTN_A); }
    else if(single_tap_list((u32)vt)) list_jump(o,idx+d,1); else list_pick(o,idx+d);
    tm_log2("list",d,(s64)vt); return 1; }
  if(p->has_idx && !widget_vis(o)) return 0;
  float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f;
  if(x<hx+p->x0 || x>hx+p->x1) return 1; // outside the list: swallow the tap (no tap-to-close, no stray A)
  float f=(y-(hy-p->off))/p->pitch; int d=(int)(f+(f>=0?0.5f:-0.5f));
  if(p->has_idx){ s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); if(cnt<=0||cnt>60||idx<0||idx>=cnt) return 1;
    if(idx+d<0||idx+d>=cnt) return 1;
    if(p->vt==0x1a21af0 && d!=0){ list_jump(o,idx+d,0); tm_log2("bulletin select",d,(s64)hy); return 1; } // bulletin board: double tap
    if(p->vt==0x1a436f8){ list_pick(o,idx+d); tm_log2("units pick",d,(s64)hy); return 1; }   // Units / Unit Info table: double tap
    list_jump(o,idx+d,1); tm_log2(p->name,d,(s64)hy); return 1; }
  u64 b=d>0?BTN_D:BTN_U; int n=d<0?-d:d; for(int i=0;i<n;i++) q_push(b,1,3);
  q_push(BTN_A,2,3); tm_log2(p->name,d,(s64)hy); return 1;
}
#include "screens.h"
// Storehouse: category tabs (Left/Right), index at G+0x92434
static int storehouse_tap(u64 g,float x,float y){
  u64 o=RD64(g+G_HAND_OWNER); if(!mem_ok(o) || RD64(o)!=g_main+0x1a48aa8 || !widget_vis(o)) return 0;
  if(y>=148.f&&y<=188.f&&x>=183.f&&x<=472.f){ s32 tab=RDS32(g+0x92434); if(tab<0||tab>8) return 1;
    int k=(int)((x-200.f)/31.8f+0.5f); if(k<0)k=0; if(k>8)k=8; if(k==tab) return 1;
    *(volatile s32*)(g+0x92434)= k>tab ? k-1 : k+1; q_push(k>tab?BTN_R:BTN_L,2,3);   // instant, like the convoy tabs
    tm_log2("store tab",k,tab); return 1; }
  if(y>=145.f&&y<=192.f&&x>=35.f&&x<=110.f){ q_push(BTN_L,1,3); return 1; }   // arrows at both ends of the icon strip
  if(y>=145.f&&y<=192.f&&x>=555.f&&x<=640.f){ q_push(BTN_R,1,3); return 1; }
  return 0;
}
// Monastery map areas. The map widget o (vtable 0x1a1a688) keeps its selectable areas at o+0x7108 (u32 area ids, count
// o+0x7100, selected index o+0x7104). main+0x2b2fa0(o, index) is the game's own select (moves the marker, refreshes the
// People list). Area records: T = *(*(main+0x1aa9450)), T+8+k*8 (k < 100) -> record, id = byte +1, map position floats
// +4/+8. The game draws the selected marker at pos*901/64000 + (182, 50) in 1080p (scale 32000 when the map is zoomed)
// and stores that at o+0x72d4/+0x72d8; that stored value picks the scale and corrects the offset.
typedef void (*MapSelFn)(u64 o,s32 idx);
static u64 map_area_rec(u32 id){ u64 p=RD64(g_main+0x1aa9450); if(!mem_ok(p)) return 0; u64 t=RD64(p); if(!mem_ok(t)) return 0;
  for(int k=0;k<100;k++){ u64 r=RD64(t+8+(u64)k*8); if(r && mem_ok(r) && *(volatile u8*)(r+1)==id) return r; } return 0; }
static int map_area_tap(u64 o,float x,float y){
  s32 n=RDS32(o+0x7100), cur=RDS32(o+0x7104); if(n<=0||n>100||cur<0||cur>=n) return 0;
  u64 rc=map_area_rec(RD32(o+0x7108+(u64)cur*4)); if(!rc) return 0;
  float mx=RDF(o+0x72d4), my=RDF(o+0x72d8), sc=901.f/64000.f;
  { float e1=RDF(rc+4)*sc+182.f-mx, e2=RDF(rc+4)*sc*2.f+182.f-mx; if(e1<0)e1=-e1; if(e2<0)e2=-e2; if(e2<e1) sc*=2.f; }
  float ox=mx-RDF(rc+4)*sc, oy=my-RDF(rc+8)*sc;          // 1080p offset (182, 50) normally
  float tx=x*1.5f, ty=y*1.5f, bd=1e9f; int best=-1;
  for(int i=0;i<n;i++){ u64 r=map_area_rec(RD32(o+0x7108+(u64)i*4)); if(!r) continue;
    float dx=RDF(r+4)*sc+ox-tx, dy=RDF(r+8)*sc+oy-ty, d=dx*dx+dy*dy; if(d<bd){ bd=d; best=i; } }
  if(best<0 || bd>75.f*75.f) return 0;                  // within 50 px (720p) of an area marker
  if(best==cur){ press(BTN_A); tm_log2("map area A",best,n); return 1; }
  ((MapSelFn)(g_main+0x2b2fa0))(o,best); tm_log2("map area",best,cur); return 1; }
// Monastery map (R): People / Facilities tabs (ZL/ZR), X Locate in the corner. The map widget is the hand owner
// (vtable main+0x1a1a688) even though the hand itself is hidden; its tab index sits 0x5ff8 bytes before it.
static int map_tap(u64 g,float x,float y){
  u64 o=RD64(g+G_HAND_OWNER); if(!mem_ok(o) || RD64(o)!=g_main+0x1a1a688 || !widget_vis(o)) return 0;
  if(y>=50.f&&y<=98.f&&x>=830.f&&x<=1240.f){ s32 tab=RDS32(o-0x5ff8); int k=-1;
    if(x<905.f) k=tab-1; else if(x<1035.f) k=0; else if(x<1150.f) k=1; else k=tab+1;
    if(k<0||k>1||tab<0||tab>1||k==tab) return 1;
    q_push(k>tab?(1ul<<9):(1ul<<8),2,4); tm_log2("map tab",k,tab); return 1; }
  // prompts: "+ View location details", "X Locate", "A Fast-travel"
  if(y>=682.f&&y<=718.f){ if(x>=736.f&&x<=922.f){ press(BTN_PLUS); return 1; } if(x>=975.f&&x<=1080.f){ press(BTN_X); return 1; }
    if(x>=1129.f&&x<=1264.f){ press(BTN_A); return 1; } return 1; }
  // the People / Facilities list on the right (vt 0x1a1a4d0, 0x64f8 bytes before the map widget; rows 53.33 apart, row 0's middle at
  // y 194 unscrolled; on Facilities the resources start below the description, row 0 at y 354 whatever its length): its index is set
  // first (SetIndex keeps it when Locate opens), then X: Locate opens with the hand already on the tapped name and the map showing
  // them, no replay. Fallback: X, then replay the tap on the list (map_list_tap).
  if(x>=860.f&&x<=1240.f&&y>=165.f&&y<=640.f){
    u64 l=o-0x64f8;
    if(mem_ok(l) && RD64(l)==g_main+0x1a1a4d0){ s32 cnt=RDS32(l+0x54), top=(s32)(short)(RD32(l+0x68)>>16);
      float y0= RDS32(o-0x5ff8)==1 ? 354.f : 194.f; if(y<y0-27.f) return 1;   // the facility's description: nothing to locate
      float f=(y-y0)/53.33f; int t=top+(int)(f+(f>=0?0.5f:-0.5f));
      if(cnt>0 && cnt<=60 && t>=0 && t<cnt){ ((SetIdxFn)(g_main+0x5de5b0))(l,t); press(BTN_X); tm_log2("map name",t,cnt); return 1; }
      if(cnt>=0 && cnt<=60) return 1; }   // below the last name (or nobody there): nothing to locate
    if(rt_depth==0){ press(BTN_X); retap_on_change(x,y,30); } return 1; }
  if(x<800.f && map_area_tap(o,x,y)) return 1;
  return 1;   // nothing else on this screen: no stray A (it asks to fast-travel)
}
// Map -> X Locate: the People / Facilities list (vt 0x1a1a4d0, rows 53.33 apart, the hand ~4 px below a row's middle). A tap selects the
// person / facility (the map shows where); A there leaves Locate, so a tap never presses it. A tap on the map goes back first (B).
static int map_list_tap(u64 g,float x,float y){
  u64 o=RD64(g+G_HAND_OWNER); if(!hand_on(g) || !mem_ok(o) || RD64(o)!=g_main+0x1a1a4d0 || !widget_vis(o)) return 0;
  if(x<800.f){ if(rt_depth==0){ press(BTN_B); retap_later(x,y,15); } return 1; }
  s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); if(cnt<=0||cnt>60||idx<0||idx>=cnt||x<860.f||x>1240.f) return 1;
  float hy=RDF(g+0x30)/1.5f, f=(y-(hy-4.f))/53.33f; int d=(int)(f+(f>=0?0.5f:-0.5f)); int t=idx+d; if(t<0||t>=cnt) return 1;
  if(t!=idx) list_jump(o,t,0); tm_log2("map locate",t,idx); return 1; }
// Birthday picker (new game intro). Its state lives outside the common UI object, so the screen is recognised by
// the help bar (G+0x2cf8, vtable main+0x1a43568, UTF-8 text at +0x3e) reading "Enter your birthday".
// Left/Right pick the Month or Day field (they don't wrap), Up/Down change the value.
#define G_HELP 0x2cf8
static int help_text_is(u64 g,const char*t){ u64 h=g+G_HELP; if(!is_widget(h,0x1a43568)) return 0;
  const volatile char*p=(const volatile char*)(h+0x3e); while(*t){ if(*p!=*t) return 0; p++; t++; } return 1; }
static int birthday_tap(u64 g,float x,float y){
  if(hand_on(g) || !help_text_is(g,"Enter your birthday")) return 0;
  if(y<240.f||y>400.f||x<410.f||x>870.f){ q_push(BTN_A,2,3); return 1; }  // outside the picker: confirm (a Yes/No follows)
  u64 side=0, step=0;
  if(x<=505.f){ side=BTN_L; if(y<300.f) step=BTN_U; else if(y>358.f) step=BTN_D; }        // month column
  else if(x>=770.f){ side=BTN_R; if(y<300.f) step=BTN_U; else if(y>358.f) step=BTN_D; }   // day column
  else if(y>=300.f&&y<=358.f){ side=BTN_L; if(x<565.f) step=BTN_U; else if(x>710.f) step=BTN_D; } // moon name with its arrows
  if(side) q_push(side,2,3); if(step) q_push(step,2,3);
  tm_log2("birthday",(s64)side,(s64)step); return 1;
}
// free roam prompts "X Menu" / "R Map" in the bottom-right corner
static int freeroam_corner_tap(u64 g,float x,float y){
  int finish= x>=28.f && x<=260.f && y>=620.f && y<=665.f;   // "L Finish Exploring" on the left (free days)
  if(!(y>655.f&&x>1005.f) && !finish) return 0;   // "X Menu" spans x 1013-1120, "R Map" 1165-1264
  u64 b=battle(); s32 ph=b?RDS32(b+0x3cc0):-1; if(hud() || (ph!=11&&ph!=26) || hand_on(g)) return 0;
  u64 o=RD64(g+G_HAND_OWNER); if(mem_ok(o) && RD64(o)==g_main+0x1a1a688 && widget_vis(o)) return 0;
  if(!is_widget(g+0x31120,0x1a44f48)) return 0; // free-roam HUD (vtable 0x1a44f48) is hidden while a conversation is up
  if(finish){ press(BTN_LB); tm_log2("finish exploring",(s64)x,ph); return 1; }
  q_push(x<1165.f?(1ul<<2):(1ul<<7),2,4); tm_log2("freeroam corner",(s64)x,ph); return 1;
}
static void on_tap(float x,float y){
  u64 b=battle(); s32 phase= b? RDS32(b+0x3cc0) : -1;
  if(tlog){ Buf bb={0}; bstr(&bb,"[touchmod] tap "); bint(&bb,(s64)x); bput(&bb,','); bint(&bb,(s64)y); bstr(&bb," phase="); bint(&bb,phase); bflush(&bb); }
  if(yesno_tap(x,y)) return;
  // message box (G+0x120, vt 0x1a42b98: "Sold ... x 1", "not enough points" ...): the hand underneath keeps its owner, so the list
  // handlers would swallow the tap; any tap closes the box (A)
  { u64 g=cmn_ui(); if(g && is_widget(g+0x120,0x1a42b98)){ press(BTN_A); tm_log2("msgbox",(s64)x,(s64)y); return; } }
  // tutorial / info pop-ups (common UI widget) are modal: checked before the screens underneath, whose list keeps the hand
  // (the tea party opens with one over its "Talk!" prompt, which swallowed every tap). Page arrows under the text turn
  // pages, anywhere else (also "B Close" in the corner) closes. Only the page flag G+0x302e1 counts here: G+0x60ab8 is also set
  // while the Guide's category / topic lists are open (1 / 2), which must keep their own taps (checked again further down).
  { u64 g=cmn_ui(); if(g && *(volatile u8*)(g+0x302e1)){
      if(y>=555.f&&y<=605.f&&x>=530.f&&x<=625.f){ q_push(BTN_L,2,3); tm_log2("tutorial page",-1,0); return; }
      if(y>=555.f&&y<=605.f&&x>=650.f&&x<=745.f){ q_push(BTN_R,2,3); tm_log2("tutorial page",1,0); return; }
      q_push(BTN_B,2,3); tm_log2("tutorial close",(s64)x,(s64)y); return; } }
  { u64 g=cmn_ui(); if(g && title_flow_tap(g,x,y)) return; }
  if(title_menu_tap(x,y)) return;
  { u64 g=cmn_ui(); if(g && (talk_tap(g,x,y) || screens_tap(g,x,y))) return; }
  if(items_tap(x,y)) return;
  { u64 g=cmn_ui(); if(g && (storehouse_tap(g,x,y) || map_tap(g,x,y) || map_list_tap(g,x,y) || hand_list_tap(g,x,y) || hl_list_tap(g,x,y) || birthday_tap(g,x,y))) return; }
  // info pop-ups flagged only by G+0x60ab8 that no screen above took: tap anywhere closes them (arrows turn pages)
  { u64 g=cmn_ui(); if(g && RDS32(g+0x60ab8)!=0){
      if(y>=555.f&&y<=605.f&&x>=530.f&&x<=625.f){ q_push(BTN_L,2,3); return; }
      if(y>=555.f&&y<=605.f&&x>=650.f&&x<=745.f){ q_push(BTN_R,2,3); return; }
      q_push(BTN_B,2,3); return; } }
  { u64 g=cmn_ui(); if(g && freeroam_corner_tap(g,x,y)) return; }
  // fighting tournament (tournament organizer): the round screen between fights (G+0x606a0, vt 0x19f2ca8), which also shows the
  // WIN / LOSE result, runs as battle phase 11, where taps pressed B (nothing happens there). Prompts in the corner: X Restore HP,
  // B Withdraw (from round 2 on), A Confirm; any other tap = A (starts the fight; the result waits for A).
  { u64 g=cmn_ui(); if(g && !hand_on(g) && is_widget(g+0x606a0,0x19f2ca8) && (phase==10||phase==11||!hud())){
      u64 bt=BTN_A; if(y>=682.f&&y<=718.f){ if(x>=718.f&&x<=920.f) bt=BTN_X; else if(x>=957.f&&x<=1100.f) bt=BTN_B; }
      press(bt); tm_log2("tournament",(s64)bt,(s64)x); return; } }
  // the battle object and phase linger outside battles (title, intro), so also require the battle HUD
  int in_battle = b && cam_obj() && RDS32(b+0x3b20)>0 && hud();
  // Divine Pulse (phase 17): the log of past actions on the right is a wheel: the selected entry stays at y 221 and the list
  // scrolls, older entries below (lower index). Each step rewinds the map with an animation, so a tap on another entry walks
  // there with real Up / Down presses (not instant); a tap on the entry reached by a tap (or tapped twice) confirms (A
  // rewinds at once, no Yes/No). Arrows above / below the selected entry step; prompts B Back / A Confirm.
  if(in_battle && phase==17){ u64 hh=hud(); u64 w= hh ? hh+0x9c930 : 0;
    if(y>=682.f && y<=718.f){ if(x>=1000.f&&x<=1095.f){ press(BTN_B); return; } if(x>=1140.f&&x<=1264.f){ press(BTN_A); return; } return; }
    if(!w || !is_widget(w,0x19f5480)) return;
    s32 idx=RDS32(w+0x4c), cnt=RDS32(w+0x54); if(cnt<=0||cnt>500||idx<0||idx>=cnt) return;
    if(x>=1228.f){ if(y>=140.f&&y<=195.f){ press(BTN_U); return; } if(y>=248.f&&y<=292.f){ press(BTN_D); return; } }
    if(x<700.f || y<140.f || y>402.f) return;
    float f=(y-221.f)/40.f; int k=(int)(f+(f>=0?0.5f:-0.5f)); int t=idx-k; if(t<0||t>=cnt) return;
    if(!k){ if(dt_same(w+0x4c,idx,0,0)){ dt_on=0; press(BTN_A); } else dt_arm(w+0x4c,idx,0,0); tm_log2("pulse confirm",idx,0); return; }
    u64 bt= k>0 ? BTN_D : BTN_U; int n= k>0 ? k : -k; for(int i=0;i<n;i++) q_push(bt,2,8);
    dt_arm(w+0x4c,t,0,0); tm_log2("pulse walk",t,idx); return; }
  // "-" zoomed all the way out (phase 18, overview map with the battle conditions): prompts ZL Log / B Back
  if(in_battle && phase==18){ u64 g=cmn_ui(); if(!(g && hand_on(g))){
      if(y>=682.f && y<=718.f){ if(x>=1020.f&&x<=1125.f){ press(BTN_ZL); return; } if(x>=1160.f&&x<=1264.f){ press(BTN_B); return; } }
      return; } }
  if(!(in_battle && phase>=0 && phase<=3)){ u64 g=cmn_ui(); if(g && picker_tap(g,x,y)) return; }
  // View Unit (+ in unit details) shows the class model with a lone "B Back" prompt: phase 15, no hand
  if(in_battle && phase==15){ u64 g=cmn_ui(); if(!(g && hand_on(g))){ if(inbox(x,y,1150.f,1264.f,682.f,718.f)) press(BTN_B); return; } }
  // battle preparation map (wheel -> Map): free cursor, A picks a unit and A on another tile swaps / moves it there.
  // battle+0x3b60 = the picked unit. Prompts "- Zoom Out" / "X Details".
  if(in_battle && phase==30){ u64 g=cmn_ui(); if(!(g && hand_on(g))){
      if(y>=682.f && y<=718.f){ if(x>=955.f&&x<=1105.f){ press(BTN_MINUS); return; } if(x>=1140.f&&x<=1264.f){ press(BTN_X); return; }
        if(x>=795.f&&x<=925.f){ press(BTN_PLUS); return; } }    // zoomed out: "+ Zoom In"
      int tx,ty; if(pick_tile(x,y,&tx,&ty)){ int cx,cy; cursor_get(&cx,&cy);
        if(cx==tx&&cy==ty){ q_push(BTN_A,2,3); return; }
        int want_a = tile_has_unit(tx,ty) || RD64(b+0x3b60)!=0;
        if(!want_a){ if(!set_cursor_instant(tx,ty)) chase_start(tx,ty,0); }
        else { // the swap target only registers after a real d-pad step (as with phase 1): land next to it, step, then A
          int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24); int done=0;
          for(int k=0;k<4&&!done;k++){ int nx=tx-dmap_dx[k], ny=ty-dmap_dy[k]; if(nx<minx||ny<miny||nx>=maxx||ny>=maxy) continue;
            if(nx==cx&&ny==cy){ q_push(dbtn[k],1,2); done=1; break; }
            if(set_cursor_instant(nx,ny)){ q_push(0,1,0); q_push(dbtn[k],1,2); done=1; } }
          if(done) wait_then_a(tx,ty); else chase_start(tx,ty,1); }
        tm_log2("prep tile",tx*100+ty,want_a); }
      return; } }
  if(in_battle && phase==0 && y>=684.f && y<=716.f){        // prompts: X Details (unit under the cursor), ZR Danger Area
    if(x>=962.f&&x<=1069.f){ press(BTN_X); return; }
    if(x>=1120.f&&x<=1264.f){ press(BTN_ZR); return; }
    if(x>=770.f&&x<=910.f){ press(BTN_ZL); return; } }       // ZL Divine Pulse (shown from turn 2 while charges remain)
  if(in_battle && (phase==0||phase==1)){
    int tx,ty; if(pick_tile(x,y,&tx,&ty)){ int cx,cy; cursor_get(&cx,&cy);
      if(tlog){ Buf bb={0}; bstr(&bb,"[touchmod] tile "); bint(&bb,tx); bput(&bb,','); bint(&bb,ty); bstr(&bb," cur "); bint(&bb,cx); bput(&bb,','); bint(&bb,cy); bflush(&bb); }
      int has_unit=tile_has_unit(tx,ty);
      int want_a = (phase!=0) || has_unit;
      if(cx==tx&&cy==ty){ move_confirmed(); q_push(BTN_A,2,3); }
      else if(phase==1 && cell_at(tx,ty) && !(*(volatile u8*)(cell_at(tx,ty)+0x26)&3)){
        // outside the blue/red range: ignore instead of confirming "stay here"
      }
      else if(phase==1){
        move_confirmed();
        // jump next to the target, then take one real d-pad step so the game recomputes the move path
        u64 bb=battle(); int minx=RDS32(bb+0x3b18), miny=RDS32(bb+0x3b1c), maxx=RDS32(bb+0x3b20), maxy=RDS32(bb+0x3b24); int done=0;
        for(int k=0;k<4&&!done;k++){ int nx=tx-dmap_dx[k], ny=ty-dmap_dy[k]; if(nx<minx||ny<miny||nx>=maxx||ny>=maxy) continue;
          if(nx==cx&&ny==cy){ q_push(dbtn[k],1,2); done=1; break; }
          if(set_cursor_instant(nx,ny)){ q_push(0,1,0); q_push(dbtn[k],1,2); done=1; } }
        if(done) wait_then_a(tx,ty); else chase_start(tx,ty,1);
      }
      else if(set_cursor_instant(tx,ty)){ if(want_a){ q_push(0,2,0); q_push(BTN_A,2,3); } }
      else chase_start(tx,ty,want_a);
      return; }
  }
  if(in_battle && phase==2){
    u64 hh=hud(); int menu_vis = hh ? (*(volatile u8*)(hh+0x1068+0x21)!=0) : 1;
    // weapon / combat art picker after Attack (HUD+0x1610, same class as the Items list): rows 35 px apart from the hand,
    // first tap selects (the stats below update), a second tap on the same weapon confirms
    { u64 g=cmn_ui(); u64 wp= hh ? hh+0x1610 : 0;
      if(g && wp && hand_on(g) && RD64(g+G_HAND_OWNER)==wp && is_widget(wp,0x19f9678)){
        s32 idx=RDS32(wp+0x4c), cnt=RDS32(wp+0x54); if(cnt<=0||cnt>20||idx<0||idx>=cnt) return;
        float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f;
        if(x<hx-20.f||x>hx+370.f){ return; }
        float f=(y-hy)/35.05f; int d=(int)(f+(f>=0?0.5f:-0.5f)); if(idx+d<0||idx+d>=cnt) return;
        // the row under the hand already shows its stats (the first weapon / art when the picker opens): a tap on it confirms;
        // another row is selected first, the next tap on it confirms
        if(!d && !q_busy()){ dt_on=0; press(BTN_A); tm_log2("weapon confirm",idx,0); return; }
        list_pick(wp,idx+d); tm_log2("weapon",idx+d,idx); return; } }
    if(menu_vis){ menu_tap(x,y); return; }
    // combat forecast (top left): ZL / ZR change the combat art, the diamonds beside the weapon switch weapons
    // combat forecast (top left): "ZL <art> ZR" row, then "X <weapon> Y" row (X / Y switch the weapon)
    { u64 g=cmn_ui(); if(g && !hand_on(g) && x<340.f && y<212.f && y>100.f){
        if(x<62.f && y<150.f) press(BTN_ZL);
        else if(x>=285.f && y>=140.f && y<172.f) press(BTN_ZR);
        else if(y<160.f) press(BTN_ZR);                       // the art name: next art
        else if(x<75.f) press(BTN_X); else press(BTN_Y);      // weapon row: X previous, the name or Y next
        tm_log2("forecast",(s64)x,(s64)y); return; } }
    // target selection (attack/talk/heal...): step the cursor between targets
    int tx,ty; if(pick_tile(x,y,&tx,&ty)){ int cx,cy; cursor_get(&cx,&cy);
      if(cx==tx&&cy==ty){ q_push(BTN_A,2,3); return; }
      if(tile_has_unit(tx,ty) && !(tx==cx&&ty==cy)){
        // weapon/item picker: cursor still on the acting unit -> any tap confirms
        u64 u=RD64(b+0x3b60); if(u && mem_ok(u) && *(volatile u8*)(u+0xf2)==(u8)cx && *(volatile u8*)(u+0xf3)==(u8)cy){ q_push(BTN_A,2,3); return; }
        // first tap selects the target instantly (forecast shows); tapping it again confirms
        if(target_select(tx,ty)) return;
        if(g_tgt_known){ tm_log2("target ignored",tx*100+ty,0); return; }   // the game says it is not a target: leave the cursor alone
        // The game's check can't answer and the tile has no target flag: walk the game's target order one step at a time until
        // the tapped unit is reached (no d-pad chase: every step jumps to the next target, which hopped between them on its own).
        tcycle_start(tx,ty); return; } }
    { int cx,cy; cursor_get(&cx,&cy); u64 u=RD64(b+0x3b60);
      if(u && mem_ok(u) && *(volatile u8*)(u+0xf2)==(u8)cx && *(volatile u8*)(u+0xf3)==(u8)cy){ q_push(BTN_A,2,3); } }
    return; }
  if(in_battle && (phase==10||phase==11)){ q_push(BTN_B,2,3); return; }
  if(in_battle && phase==3){ menu_tap(x,y); return; }
  q_push(BTN_A,2,3);
}
// ---- hold to fast-forward ----
// Holding A speeds up the enemy phase, unit movement and combat. A touch held still for HOLD_N polls while the
// player has no control in a battle (not phases 0-3 / 15) holds A until the finger lifts; the release is then not a tap.
#define HOLD_N 15
static u64 t_p0;
static int hold_ok(void){
  u64 b=battle(); if(!b || !cam_obj() || RDS32(b+0x3b20)<=0 || !hud()) return 0;
  s32 ph=RDS32(b+0x3cc0); if(ph<=3 || ph==15 || ph==16 || ph==17 || ph==18 || (ph>=28&&ph<=31)) return 0;   // 16 unit details, 17 Divine Pulse (A there rewinds at once)   // player-controlled screens (prep 28-31, overview 18)
  u64 g=cmn_ui(); if(g && widget_vis(g+0xe00)) return 0;           // Yes/No dialog
  if(g && (*(volatile u8*)(g+0x302e1) || RDS32(g+0x60ab8)!=0)) return 0; // tutorial pop-up
  return 1; }
// Instruct: the topic list (vt 0x1a1ddc8, rows 53.3 apart from the hand). A finger held on a row selects it and then taps A every
// 14 polls until the finger lifts: that keeps instructing the topic (through the lines and "Praise" in between, then the same
// topic again while sessions remain). A held A alone does not repeat in these menus.
static int hold_wait, hold_mash; static u64 hold_p0;
// A hold anywhere works: on a topic row it picks that topic, elsewhere it keeps the selected one. It also starts while the instruction
// lines play (no hand, G+0x58 still = the topic list) and on the Praise / Admonish choice that follows a session.
static int hold_menu(void){
  u64 g=cmn_ui(); if(!g) return 0; u64 o=RD64(g+G_HAND_OWNER); if(!mem_ok(o)) return 0; u64 vt=RD64(o)-g_main;
  if(vt==0x1a1ddc8 && !hand_on(g)){ hold_wait=0; return 1; }                      // a session is playing: keep going
  if(!hand_on(g) || !widget_vis(o)) return 0;
  if(vt==0x1a083f0 && RDS32(o+0x54)<=2){                                           // Praise / Admonish after a session
    for(int i=0;i<4;i++){ u64 h=own_hist[i]; if(h && mem_ok(h) && RD64(h)==g_main+0x1a1ddc8){ hold_wait=0; return 1; } } return 0; }
  if(vt!=0x1a1ddc8) return 0;
  s32 idx=RDS32(o+0x4c), cnt=RDS32(o+0x54); if(cnt<=0||cnt>40||idx<0||idx>=cnt) return 0;
  float hx=RDF(g+0x2c)/1.5f, hy=RDF(g+0x30)/1.5f; int it=idx;
  if(t_x>=hx-30.f && t_x<=hx+460.f){ float f=(t_y-hy)/53.33f; int d=(int)(f+(f>=0?0.5f:-0.5f)); if(idx+d>=0 && idx+d<cnt) it=idx+d; }   // on a topic row: that topic
  q_clear(); if(it!=idx) list_jump(o,it,0); hold_wait= it!=idx ? 3 : 0; return 1; }
static u64 hold_tick(void){
  if(!t_down){ return 0; }
  if(!t_long && !t_swiped && t_settle==0 && g_poll-t_p0>=HOLD_N){
    if(hold_ok()){ t_long=1; q_clear(); hold_wait=0; hold_mash=0; tm_log2("hold A",(s64)(g_poll-t_p0),0); }
    else if(hold_menu()){ t_long=1; hold_mash=1; hold_p0=0; tm_log2("hold instruct",(s64)t_y,0); } }
  if(!t_long) return 0;
  if(hold_wait>0){ if(!q_busy()) hold_wait--; return 0; }   // the row step goes first
  if(hold_mash){ if(!hold_p0) hold_p0=g_poll;
    // stop once the hand is on anything but the topics or the Praise / Admonish choice (back on the unit list: no more sessions)
    u64 g=cmn_ui(); if(g && hand_on(g)){ u64 o=RD64(g+G_HAND_OWNER); u64 vt= mem_ok(o) ? RD64(o)-g_main : 0;
      if(!(vt==0x1a1ddc8 || (vt==0x1a083f0 && RDS32(o+0x54)<=2))){ hold_mash=0; t_long=2; tm_log2("hold instruct end",(s64)vt,0); return 0; } }
    return ((g_poll-hold_p0)%14)<4 ? BTN_A : 0; }
  if(t_long==2) return 0;
  return BTN_A; }
// ---- drag = right stick (camera in free roam, model rotation in Unit Appearance) ----
// Decided at touch-down: in these screens a drag never makes d-pad swipes; finger speed is turned into a right-stick
// deflection each poll (dragging right turns the camera left, the world follows the finger). A touch that moved
// less than CAM_TAP px is still a tap.
#define CAM_TAP 14.f
static int t_cam; static float cam_lx, cam_ly, cam_vx, cam_vy, cam_dist; static s32 g_rs_x, g_rs_y;
static int cam_screen(void){ u64 g=cmn_ui(); if(!g) return 0;
  if(appearance_screen(g)) return 2;
  // Certifications / Reclass class list (vt 0x1a1ee00) with the unit's model in the middle ("R Rotate"): a drag that starts on the
  // model turns it like Unit Appearance (the list on the left keeps its taps; a touch that doesn't move is still a tap)
  { u64 o; if(hand_on(g) && hand_owner_vt(g,&o)==0x1a1ee00 && widget_vis(o) && t_x>=330.f && t_x<=900.f && t_y>=90.f && t_y<=650.f) return 2; }
  if(hand_on(g)) return 0;
  if(is_widget(g+0x31120,0x1a44f48)) return 1;    // free-roam HUD up: Byleth is under direct control
  { u64 b=battle(), h=hud(); s32 ph= b ? RDS32(b+0x3cc0) : -1;     // battle map, nothing open: drag moves the map cursor
    if(b && h && cam_obj() && RDS32(b+0x3b20)>0 && (ph==0||ph==1||ph==30) && !widget_vis(h+0x1068) && !widget_vis(h+0xab8)
       && !widget_vis(g+0xe00) && !*(volatile u8*)(g+0x302e1) && RDS32(g+0x60ab8)==0 && !is_widget(g+0xf1eb4,0x1a2e688)) return 3; }
  return 0; }
// Battle cursor drag (t_cam 3): every poll the tile under the finger is picked on the grid plane; when it changes the
// cursor jumps there with the game's SetCursor (phase 0), or lands next to it and takes one real d-pad step so the
// move path is recomputed (phase 1, unit selected). A drag never confirms anything; lifting the finger is not a tap.
static int dc_x=-1, dc_y=-1;
static void drag_cursor_tick(void){
  if(q_busy()) return; u64 b=battle(); if(!b) return; s32 ph=RDS32(b+0x3cc0); if(ph!=0&&ph!=1&&ph!=30) return;
  // the camera follows the cursor, so pick in the view the drag started with (else the cursor runs away from the finger)
  if(!g_cfrozen) cam_freeze(1);
  int tx,ty, ok=pick_tile_any(t_x,t_y,&tx,&ty); if(!ok) return;
  if(tx==dc_x&&ty==dc_y) return; int cx,cy; cursor_get(&cx,&cy); dc_x=tx; dc_y=ty; if(cx==tx&&cy==ty) return;
  if(ph==0||ph==30){ set_cursor_instant(tx,ty); return; }
  int minx=RDS32(b+0x3b18), miny=RDS32(b+0x3b1c), maxx=RDS32(b+0x3b20), maxy=RDS32(b+0x3b24);
  for(int k=0;k<4;k++){ int nx=tx-dmap_dx[k], ny=ty-dmap_dy[k]; if(nx<minx||ny<miny||nx>=maxx||ny>=maxy) continue;
    if(nx==cx&&ny==cy){ q_push(dbtn[k],1,1); return; }
    if(set_cursor_instant(nx,ny)){ q_push(0,1,0); q_push(dbtn[k],1,1); return; } }
  set_cursor_instant(tx,ty); }
static s32 cam_axis(float v,float gain){ float a=v<0?-v:v; if(a<0.25f) return 0; float o=7000.f+a*gain; if(o>32767.f) o=32767.f; return (s32)(v<0?-o:o); }
static void cam_tick(void){
  if(!t_down||!t_cam||t_settle>0){ g_rs_x=g_rs_y=0; cam_vx=cam_vy=0.f; return; }
  float dx=t_x-cam_lx, dy=t_y-cam_ly; cam_lx=t_x; cam_ly=t_y;
  cam_vx=cam_vx*0.6f+dx*0.4f; cam_vy=cam_vy*0.6f+dy*0.4f;
  float tx=t_x-t_x0, ty=t_y-t_y0; float d2=tx*tx+ty*ty; if(d2>cam_dist) cam_dist=d2;
  if(cam_dist<CAM_TAP*CAM_TAP){ g_rs_x=g_rs_y=0; return; }
  if(t_cam==3){ g_rs_x=g_rs_y=0; drag_cursor_tick(); return; }
  if(t_cam==2){ g_rs_x=cam_axis(cam_vx,1400.f); g_rs_y=0; }          // model turntable: horizontal only
  else { // free roam: horizontal 18x the finger speed (2x the old 9x), vertical 0.75x of the old response (gain and top deflection)
    g_rs_x=cam_axis(-cam_vx*18.f,2000.f); g_rs_y=(s32)((float)cam_axis(cam_vy*9.f,1400.f)*0.75f); }
}
static void touch_update(TouchScreenState1*ts){
  u64 now=ticks();
  if(ts->count>0){
    float x=(float)ts->t[0].x, y=(float)ts->t[0].y;
    if(!t_down){ g_touch_mode=1; t_down=1; t_settle=2; t_p0=g_poll; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; t_t0=now; t_long=0; t_swiped=0;
      t_cam=cam_screen(); dc_x=dc_y=-1; cam_freeze(0); cam_lx=x; cam_ly=y; cam_vx=cam_vy=0.f; cam_dist=0.f; return; }
    if(t_settle>0){ t_settle--; t_x0=t_x=t_sx=x; t_y0=t_y=t_sy=y; cam_lx=x; cam_ly=y; return; }
    t_x=x; t_y=y;
    if(t_cam) return;                     // camera drag: no d-pad swipes
    float dx=x-t_sx, dy=y-t_sy;
    // swipe steps: every 70px of travel emits a dpad press
    if(!t_long){
      if(dy<=-70.f){ q_push(BTN_U,1,1); t_sy-=70.f; t_swiped=1; }
      else if(dy>=70.f){ q_push(BTN_D,1,1); t_sy+=70.f; t_swiped=1; }
      else if(dx<=-90.f){ q_push(BTN_L,1,1); t_sx-=90.f; t_swiped=1; }
      else if(dx>=90.f){ q_push(BTN_R,1,1); t_sx+=90.f; t_swiped=1; }
      if(t_swiped==1){ t_swiped=2; tm_log2("swipe",(s64)dx,(s64)dy); }
    }
  } else if(t_down){
    t_down=0;
    if(t_long){ t_long=0; return; }   // end of a hold: not a tap
    if(t_cam){ t_cam=0; g_rs_x=g_rs_y=0; cam_freeze(0); if(cam_dist>=CAM_TAP*CAM_TAP) return; }   // a camera drag is not a tap
    if(!t_swiped){ int held= q_phase==1 && q_cur; chase_on=0; tc_on=0; wa_on=0; rt_n=0; hc_on=0; q_clear();
      if(held) q_push(0,1,1);   // a button from the previous tap is still down: release it first, or the two presses merge
      on_tap(t_x,t_y); }
  }
}
