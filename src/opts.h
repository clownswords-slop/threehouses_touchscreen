// ---- "TouchMod" tab in the Options screen ----
// The Options list (G+0x2250, vtable main+0x1a431f8) shows option IDs per tab: the tab byte is list+0x980 (0 Battle, 1 Monastery,
// 2 General), the IDs u16 at list+0x84 (count list+0x80), values at list+0x500 + 4*ID (IDs 0..0x21). The tab header object
// (G+0x60770) has 5 tab slots (G+0x60870 + i*0x88; the message file still has "Sound/Event" / "Other" tab names); Options opens it
// with 3 (count G+0x60a9c). Patches (main offsets, each checked against the original instruction first):
//   0x5fc8d4  tab count 3 -> 4                       0x5fd52c / 0x5fd570  ZR / ZL wrap after tab 3 instead of 2
//   0x5fd5bc  tab 3 gets IDs 0x22 (ours) and 0x23, then 0x5fd698 cuts it to one row (the ID copy loop needs at least 2)
//   0x5fd288 / 0x5fd80c / 0x5fd838 / 0x5fd9a4  per-tab scroll (list+0x974 +
//             4*tab) for tabs 0..2 only: tab 3 would land on the tab byte itself
//   0x669928  tab 3's label (GetMessage(0xff)) -> "TouchMod"
//   0x5fe7f8  row label GetMessage(table[ID]) -> "Tap To Move" for ID 0x22 (ID in x25)
//   0x5fe784  value text (main+0x5fe4d0) for ID 0x22     0x5fe04c  description (main+0x5fe020 returns early for IDs > 0x21)
//   0x5fd128  Restore defaults -> Yes resets Tap To Move too
// Left/right on the row are handled here (the game's own handler would write list+0x500+4*0x22, another field).
#define OPT_ID 0x22
#define GETMSG_OFF 0x3f7de0
#define VALTEXT_OFF 0x5fe4d0
#define DESC_OFF 0x5fe020

static const char* const tap_mode_name[3]={ "Normal", "Instant", "Instant when safe" };
static const char* const tap_mode_desc[3]={
  "Tapping a tile with a unit selected does not confirm movement.",
  "Tapping a tile with a unit selected confirms movement.",
  "Tapping a tile with a unit selected confirms movement if it would not place the unit in range of an enemy's attack." };
#define HIDDEN __attribute__((visibility("hidden")))
HIDDEN const char tm_str_tab[]="TouchMod";
HIDDEN const char tm_str_row[]="Tap To Move";
HIDDEN u64 g_fn_getmsg, g_fn_valtext, g_fn_reload;   // used by the stubs

HIDDEN void tm_opt_value_text(char*buf){ const char*s=tap_mode_name[g_tap_mode]; int i=0; while(s[i]&&i<0x1ff){ buf[i]=s[i]; i++; } buf[i]=0; }
HIDDEN void tm_opt_desc(void){ u64 g=RD64(g_main+0x21d9ae0); if(!g) return; volatile char*d=(volatile char*)(g+0x2d36);
  const char*s=tap_mode_desc[g_tap_mode]; int i=0; while(s[i]&&i<0x1ff){ d[i]=s[i]; i++; } while(i<0x200) d[i++]=0; }

// The stubs are reached with `bl` from the patched instruction (every host function saves x30 and makes calls of its own, so x30
// is free there); x16 is a scratch register (not used by any of the host functions).
asm(
".text\n"
".hidden tm_stub_fill\n .hidden tm_stub_cnt\n .hidden tm_stub_sstore\n .hidden tm_stub_sload\n .hidden tm_stub_tablabel\n"
".hidden tm_stub_rowlabel\n .hidden tm_stub_value\n .hidden tm_stub_desc\n .hidden tm_stub_restore\n"
".global tm_stub_fill\n tm_stub_fill:\n"          // "mov x8, xzr" (+ "mov w20, #0x11" -> nop): first ID / count for the tab in w20
"  and w16, w20, #0xff\n  cmp w16, #3\n  b.eq 1f\n  mov x8, xzr\n  mov w20, #0x11\n  ret\n"
"1: mov x8, #0x22\n  mov w20, #2\n  ret\n"           // 2 IDs: the ID copy loop after the fill is unrolled by two and never ends for 1
".global tm_stub_cnt\n tm_stub_cnt:\n"              // "lsl w2, w20, #3" right after that loop: the TouchMod tab keeps one row
"  ldrb w16, [x19, #0x980]\n  cmp w16, #3\n  b.ne 1f\n  mov w20, #1\n1: lsl w2, w20, #3\n  ret\n"
".global tm_stub_sstore\n tm_stub_sstore:\n"      // "str w23, [x8, #0x974]", x8 = list + 4*tab
"  sub x16, x8, x19\n  cmp x16, #12\n  b.eq 1f\n  str w23, [x8, #0x974]\n1: ret\n"
".global tm_stub_sload\n tm_stub_sload:\n"        // "ldr w8, [x8, #0x974]"
"  sub x16, x8, x19\n  cmp x16, #12\n  b.eq 1f\n  ldr w8, [x8, #0x974]\n  ret\n1: mov w8, wzr\n  ret\n"
".global tm_stub_tablabel\n tm_stub_tablabel:\n"
"  adrp x0, tm_str_tab\n  add x0, x0, :lo12:tm_str_tab\n  ret\n"
".global tm_stub_rowlabel\n tm_stub_rowlabel:\n"  // GetMessage(table[ID]), ID in x25
"  cmp x25, #0x22\n  b.ne 1f\n  adrp x0, tm_str_row\n  add x0, x0, :lo12:tm_str_row\n  ret\n"
"1: adrp x16, g_fn_getmsg\n  ldr x16, [x16, :lo12:g_fn_getmsg]\n  br x16\n"
".global tm_stub_value\n tm_stub_value:\n"        // main+0x5fe4d0(list, ID, buf, ...): text -> buf, returns 0 (text, no gauge)
"  cmp w1, #0x22\n  b.ne 1f\n  stp x29, x30, [sp, #-16]!\n  mov x0, x2\n  bl tm_opt_value_text\n  ldp x29, x30, [sp], #16\n  mov w0, wzr\n  ret\n"
"1: adrp x16, g_fn_valtext\n  ldr x16, [x16, :lo12:g_fn_valtext]\n  br x16\n"
".global tm_stub_desc\n tm_stub_desc:\n"          // "cmp w8, #0x21" before "b.hi <return>", ID in w8
"  cmp w8, #0x22\n  b.ne 1f\n  stp x29, x30, [sp, #-16]!\n  stp x8, x9, [sp, #-16]!\n  bl tm_opt_desc\n  ldp x8, x9, [sp], #16\n  ldp x29, x30, [sp], #16\n"
"1: cmp w8, #0x21\n  ret\n"
".global tm_stub_restore\n tm_stub_restore:\n"    // "bl main+0x5fc920" (reload the option values) after Restore defaults -> Yes
"  stp x29, x30, [sp, #-16]!\n  stp x0, x1, [sp, #-16]!\n  bl tm_opt_restore\n  ldp x0, x1, [sp], #16\n  ldp x29, x30, [sp], #16\n"
"  adrp x16, g_fn_reload\n  ldr x16, [x16, :lo12:g_fn_reload]\n  br x16\n"
);
HIDDEN void tm_stub_fill(void); HIDDEN void tm_stub_sstore(void); HIDDEN void tm_stub_sload(void); HIDDEN void tm_stub_tablabel(void);
HIDDEN void tm_stub_rowlabel(void); HIDDEN void tm_stub_value(void); HIDDEN void tm_stub_desc(void); HIDDEN void tm_stub_cnt(void); HIDDEN void tm_stub_restore(void);

typedef struct { u32 off, orig; void (*stub)(void); u32 insn; } CodePatch;   // stub: write `bl stub`, else insn
static const CodePatch opt_patches[]={
  {0x5fc8d4,0x320007eb,0,0x5280008b},              // orr w11, wzr, #3 -> mov w11, #4
  {0x5fd52c,0x7100091f,0,0x71000d1f},              // cmp w8, #2 -> cmp w8, #3
  {0x5fd570,0x321f03e8,0,0x52800068},              // orr w8, wzr, #2 -> mov w8, #3
  {0x5fd5bc,0xaa1f03e8,tm_stub_fill,0},
  {0x5fd5c0,0x52800234,0,0xd503201f},              // mov w20, #0x11 -> nop
  {0x5fd698,0x531d7282,tm_stub_cnt,0},             // lsl w2, w20, #3
  {0x5fd288,0xb9097517,tm_stub_sstore,0},
  {0x5fd80c,0xb9497508,tm_stub_sload,0},
  {0x5fd838,0xb9497508,tm_stub_sload,0},
  {0x5fd9a4,0xb9497508,tm_stub_sload,0},
  {0x669928,0x97f6392e,tm_stub_tablabel,0},
  {0x5fe7f8,0x97f7e57a,tm_stub_rowlabel,0},
  {0x5fe784,0x97ffff53,tm_stub_value,0},
  {0x5fe04c,0x7100851f,tm_stub_desc,0},
  {0x5fd128,0x97fffdfe,tm_stub_restore,0},          // bl main+0x5fc920 in the Restore defaults -> Yes path
};
static int g_opts_state;   // 0 not tried, 1 patched, -1 left alone (another game version, or code writes refused)
static void opts_install(void){
  if(g_opts_state) return; g_opts_state=-1;
  for(u32 i=0;i<NP(opt_patches);i++){ const CodePatch*p=&opt_patches[i]; if(RD32(g_main+p->off)!=p->orig) return;
    if(p->stub && !a64_in_range(g_main+p->off,(u64)p->stub)) return; }
  g_fn_getmsg=g_main+GETMSG_OFF; g_fn_valtext=g_main+VALTEXT_OFF; g_fn_reload=g_main+0x5fc920;
  for(u32 i=0;i<NP(opt_patches);i++){ const CodePatch*p=&opt_patches[i]; u64 at=g_main+p->off;
    u32 v= p->stub ? a64_branch(at,(u64)p->stub,1) : p->insn;
    if(!code_write32(at,v)){ Buf b={0}; bstr(&b,"[touchmod] options patch failed at "); bhex(&b,p->off); bstr(&b," rc "); bhex(&b,g_patch_rc); bflush(&b); return; } }
  g_opts_state=1; }

// ---- settings file: sd:/touchmod/settings.txt, "tap_to_move=N" ----
#define SETTINGS_PATH "sd:/touchmod/settings.txt"
static void settings_load(void){
  if(sd_mount()) return; FH h; if(FsOpen(&h,SETTINGS_PATH,1)) return;
  char b[256]; s64 sz=0; FsSize(&sz,h); if(sz>255) sz=255; u64 got=0; WOpt ro={0}; if(sz<=0||FsReadN(&got,h,0,b,(u64)sz,&ro)) got=0; FsClose(h); b[got]=0;
  const char*k="tap_to_move="; for(u64 i=0;i+12<got;i++){ int j=0; while(k[j]&&b[i+j]==k[j]) j++;
    if(!k[j]){ int v=b[i+j]-'0'; if(v>=0&&v<=2) g_tap_mode=v; break; } } }
static void settings_save(void);
HIDDEN void tm_opt_restore(void){ if(g_tap_mode){ g_tap_mode=0; settings_save(); } }
static void settings_save(void){
  if(sd_mount()) return; FsMkdir("sd:/touchmod");
  char b[32]="tap_to_move=0\n"; b[12]=(char)('0'+g_tap_mode); u64 n=14;
  FsDelete(SETTINGS_PATH); if(FsCreate(SETTINGS_PATH,(s64)n)) return; FH h; if(FsOpen(&h,SETTINGS_PATH,2)) return;
  WOpt o={1}; FsWrite(h,0,b,n,&o); FsClose(h); }

// ---- per frame ----
#define OPT_TAB_BYTE 0x980
static u64 opt_list(void){ u64 g=cmn_ui(); if(!g) return 0; u64 w=g+0x2250; return is_widget(w,0x1a431f8) ? w : 0; }
static int opt_on_row(void){   // our tab, our row selected, no confirmation (Restore defaults) in progress
  u64 w=opt_list(); if(!w) return 0; return *(volatile u8*)(w+OPT_TAB_BYTE)==3 && RDS32(w+0x54)==1 && RDS32(w+0x4c)==0
    && !*(volatile u8*)(w+0x981) && !*(volatile u8*)(w+0x982) && !*(volatile u8*)(w+0x983); }
static void opt_set(int v){ if(v<0) v=2; if(v>2) v=0; if(v==g_tap_mode) return; g_tap_mode=v; settings_save();
  u64 w=opt_list(); u64 g=cmn_ui(); if(w&&g){ tm_opt_desc(); u64 d=g+0x2cf8; ((void(*)(u64,int,int))RD64(RD64(d)+0x78))(d,1,1); } }   // redraw the description
// Tab header: 4 tabs centred like the game's 3 (layout x = screen x - 960, y = 540 - 196); each tab element is
// *( *(G+0x60870+i*0x88) + 0x18 ), position +0x30/+0x34, flags +0x58 (bit 4 = position changed)
static const float opt_tab_x[4]={ 690.f, 870.f, 1050.f, 1230.f };
static void opt_header_tick(void){
  u64 g=RD64(g_main+0x21d9ae0); if(!g || RDS32(g+0x60a9c)!=4 || !opt_list()) return;
  for(int i=0;i<4;i++){ u64 sl=RD64(g+0x60870+i*0x88); if(!mem_ok(sl)) continue; u64 e=RD64(sl+0x18); if(!mem_ok(e)) continue;
    float x=opt_tab_x[i]-960.f, y=344.f; if(RDF(e+0x30)!=x||RDF(e+0x34)!=y){ *(volatile float*)(e+0x30)=x; *(volatile float*)(e+0x34)=y; *(volatile u8*)(e+0x58)|=0x10; } } }
#define OPT_LEFT  ((1ul<<12)|(1ul<<16)|(1ul<<20))   // d-pad, left stick, right stick (emulated buttons)
#define OPT_RIGHT ((1ul<<14)|(1ul<<18)|(1ul<<22))
static u64 g_opt_mask, g_opt_prev;
static int g_settings_loaded;
static void opts_tick(void){
  if(!g_settings_loaded){ g_settings_loaded=1; settings_load(); }
  opts_install();
  if(g_opts_state!=1){ g_opt_mask=0; return; }
  opt_header_tick();
  g_opt_mask= opt_on_row() ? (OPT_LEFT|OPT_RIGHT) : 0; }
// called with the final buttons of the primary controller (after injected presses): value change on a new press
static void opts_buttons(u64 b){
  if(g_opt_mask){ u64 nb=b&~g_opt_prev; if(nb&OPT_LEFT) opt_set(g_tap_mode-1); else if(nb&OPT_RIGHT) opt_set(g_tap_mode+1); }
  g_opt_prev=b; }
// touch (options_tap): the TouchMod tab is showing / the tapped row is our option; a tap on its left arrow lowers the value,
// anywhere else on the value side raises it (as the game's rows: left arrow 655-735, value / right arrow from 770)
static int opts_tabs4(void){ u64 g=RD64(g_main+0x21d9ae0); return g_opts_state==1 && g && RDS32(g+0x60a9c)==4; }
static int opts_our_row(int it){ u64 w=opt_list(); return w && g_opts_state==1 && *(volatile u8*)(w+OPT_TAB_BYTE)==3 && it==0; }
static void opts_tap_value(float x){ if(x>=655.f&&x<=735.f) opt_set(g_tap_mode-1); else if(x>=770.f) opt_set(g_tap_mode+1); }
