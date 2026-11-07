#include "util.h"
typedef struct { s32 x,y; } Stick;
typedef struct { s64 sampling; u64 buttons; Stick l,r; u32 attr; u32 pad; } NpadState;
typedef struct { s64 dt; u32 attr; u32 finger; s32 x,y; s32 dx,dy; s32 rot; s32 res; } TouchState;
typedef struct { s64 sampling; s32 count; s32 res; TouchState t[1]; } TouchScreenState1;

#define IMPORT(ret,name,alias,...) extern char name[]; typedef ret (*alias##_t)(__VA_ARGS__); static inline alias##_t alias##_p(void){ void*p; asm volatile("adrp %0, :got:" #name "\n ldr %0, [%0, :got_lo12:" #name "]":"=r"(p)); return (alias##_t)p; }
IMPORT(int,_ZN2nn3hid13GetNpadStatesEPNS0_16NpadFullKeyStateEiRKj,GetStatesFull,NpadState*,int,const u32*)
IMPORT(int,_ZN2nn3hid13GetNpadStatesEPNS0_17NpadHandheldStateEiRKj,GetStatesHand,NpadState*,int,const u32*)
IMPORT(int,_ZN2nn3hid13GetNpadStatesEPNS0_16NpadJoyDualStateEiRKj,GetStatesDual,NpadState*,int,const u32*)
IMPORT(void,_ZN2nn3hid21InitializeTouchScreenEv,InitTouch,void)
IMPORT(void,_ZN2nn3hid19GetTouchScreenStateILm1EEEvPNS0_16TouchScreenStateIXT_EEE,GetTouch1,TouchScreenState1*)
IMPORT(int,_ZN2nn3hid20GetTouchScreenStatesILm1EEEiPNS0_16TouchScreenStateIXT_EEEi,GetTouchN,TouchScreenState1*,int)
#define GetTouchN GetTouchN_p()
#define GetStatesFull GetStatesFull_p()
#define GetStatesHand GetStatesHand_p()
#define GetStatesDual GetStatesDual_p()
#define InitTouch InitTouch_p()
#define GetTouch1 GetTouch1_p()

#include "fs.h"
#ifdef TM_DEBUG
#include "dbg.h"
#else
typedef struct { u64 addr, size; u32 type, attr, perm, ipc, dev, pad; } MemInfo;
static inline u32 svcQueryMemory(MemInfo* mi, u32* pi, u64 addr){ register u64 x0 asm("x0")=(u64)mi; register u64 x1 asm("x1")=0; register u64 x2 asm("x2")=addr; asm volatile("svc 0x6":"+r"(x0),"+r"(x1),"+r"(x2)::"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); *pi=(u32)x1; return (u32)x0; }
#endif
#include "patch.h"
#include "touch.h"
#include "opts.h"
#ifndef TM_DEBUG
#define TLOG_OFF 1
#endif

#ifdef TM_DEBUG
static s32 dbg_ls_x, dbg_ls_y; static int dbg_ls_n;   // debug: left stick override for n polls
static s32 dbg_rs_x, dbg_rs_y; static int dbg_rs_n, dbg_rs_tot; static float dbg_yaw0, dbg_pit0, dbg_dyaw, dbg_dpit;   // debug: right stick override + camera angle change
static float atan2_approx(float y,float x){ // good to ~0.005 rad
  float ax=x<0?-x:x, ay=y<0?-y:y; if(ax<1e-9f&&ay<1e-9f) return 0.f; float a=ax>ay?ay/ax:ax/ay; float s=a*a; float r=((-0.0464964749f*s+0.15931422f)*s-0.327622764f)*s*a+a;
  if(ay>ax) r=1.57079637f-r; if(x<0) r=3.14159274f-r; if(y<0) r=-r; return r; }
static void cam_angles(float*yaw,float*pit){ u64 c=RD64(g_main+0x1ab8480); *yaw=0; *pit=0; if(!c||!mem_ok(c)) return; float dx=RDF(c+0x80)-RDF(c+0x90), dy=RDF(c+0x84)-RDF(c+0x94), dz=RDF(c+0x88)-RDF(c+0x98);
  *yaw=atan2_approx(dz,dx); float h=dx*dx+dz*dz; float hs=h; for(int i=0;i<20;i++){ if(hs<=0) break; hs=0.5f*(hs+h/hs); } *pit=atan2_approx(dy,hs); }
static u64 pf_n, pf_sum, pf_max, pf_hist[6], pf_dbg, pf_sec[16], pf_memq, pf_cur[16], pf_maxsec[16];   // perf: polls, ticks (19.2 MHz) in process() without the debug channel, max, histogram (<5/10/20/50/100/more us)
__attribute__((noinline)) int pt_fn(void){ return 1; }   // code-patching test target (ptest)
__attribute__((noinline)) static int pt_fn_call(void){ int (*volatile f)(void)=pt_fn; return f(); }
// synthetic drag: "drag x0 y0 x1 y1 n [hold]" moves a finger from (x0,y0) to (x1,y1) over n polls (720p), keeps it there for
// `hold` polls, then lifts it; fed to touch_update instead of the real touch screen while it runs
static int dg_n, dg_i, dg_hold; static float dg_x0, dg_y0, dg_x1, dg_y1; static s64 dg_samp;
static int drag_poll(void){ if(dg_n<=0) return 0;
  TouchScreenState1 f; for(u32 i=0;i<sizeof(f);i++) ((u8*)&f)[i]=0; f.sampling=++dg_samp+(1ll<<40);
  int tot=dg_n+dg_hold;
  if(dg_i<tot){ float t= dg_i>=dg_n ? 1.f : (float)dg_i/(float)(dg_n>1?dg_n-1:1); f.count=1; f.t[0].x=(s32)(dg_x0+(dg_x1-dg_x0)*t); f.t[0].y=(s32)(dg_y0+(dg_y1-dg_y0)*t); }
  else f.count=0;
  touch_update(&f); if(++dg_i>tot){ dg_n=0; } return 1; }
// per-poll recorder: "rec base o1 o2 o3 o4 n [tx ty]" samples 4 u32/floats at base+o_i (plus the injected buttons) for n polls,
// optionally tapping (tx,ty) on the first one; "recd" prints them
#define REC_MAX 400
static u64 rec_base, rec_off[4]; static u32 rec_v[REC_MAX][5]; static int rec_n, rec_i, rec_tap, rec_tx, rec_ty;
static void rec_poll(u64 inj){ if(rec_i>=rec_n) return; if(rec_tap){ rec_tap=0; dbg_tap_x=rec_tx; dbg_tap_y=rec_ty; dbg_tap=1; }
  for(int k=0;k<4;k++) rec_v[rec_i][k]=RD32(rec_base+rec_off[k]); rec_v[rec_i][4]=(u32)inj; rec_i++; }
// extra debug commands that need the touch logic
static void dbg_ext(const char*p){
  if(starts(p,"pick")){ p+=4; float x=(float)(s64)parse_num(&p), y=(float)(s64)parse_num(&p); int i=-1,j=-1,oi_=-1,oj=-1; float fx=0,fz=0;
    int r1=pick_tile_plane(x,y,&i,&j,&fx,&fz), r2=pick_tile_old(x,y,&oi_,&oj);
    os("plane "); oi(r1); oc(' '); oi(i); oc(','); oi(j); os(" frac "); of(fx); oc(' '); of(fz); os(" | old "); oi(r2); oc(' '); oi(oi_); oc(','); oi(oj);
    u64 c=cam_obj(); if(c){ os(" h="); of(RDF(c+0x94)); }
    int bi=-1,bj=-1; int r3=pick_tile_box(x,y,&bi,&bj); os(" | box "); oi(r3); oc(' '); oi(bi); oc(','); oi(bj); os(" hit="); oi(g_box_hit); oc('\n');
    float sx,sy; int cx,cy; if(battle()){ cursor_get(&cx,&cy); if(tile_screen(cx,cy,&sx,&sy)){ os("cursor "); oi(cx); oc(','); oi(cy); os(" at "); of(sx); oc(' '); of(sy); oc('\n'); } } }
  else if(starts(p,"wscan")){ p+=5; u64 base=parse_num(&p), lo=parse_num(&p), hi=parse_num(&p); int n=0;
    u64 vlo=g_main+0x19e4000, vhi=g_main+0x1ab6000;
    for(u64 o=lo;o<hi&&n<400;o+=4){ u64 a=base+o; if((a&0xfff)==0 && !mem_ok(a)){ o+=0xffc; continue; } u64 v=RD64(a); if(v>=vlo&&v<vhi && *(volatile u8*)(a+0x21)==1){ ox(o); os(" vt=+"); ox(v-g_main); os(" idx="); oi(RDS32(a+0x4c)); os(" cnt="); oi(RDS32(a+0x54)); oc('\n'); n++; } } os("n="); oi(n); oc('\n'); }
  else if(starts(p,"bsnap")){ p+=5; u32 lo=(u32)parse_num(&p), hi=(u32)parse_num(&p); ncand=0; cand_sz=1; u64 addr=0; MemInfo mi; u32 pi; u64 tot=0;
    while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(is_rw(&mi) && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo) && mi.type!=0xB && mi.type!=0xC){
        u64 b0=mi.addr<sr_lo?sr_lo:mi.addr; u64 e=mi.addr+mi.size; if(e>sr_hi) e=sr_hi; for(u64 a=b0;a<e;a++){ u32 w=*(volatile u8*)a; if(w>=lo&&w<=hi){ tot++; if(ncand<MAXC){ cand[ncand]=a; candv[ncand]=w; ncand++; } } } }
      u64 n=mi.addr+mi.size; if(n<=addr||!n) break; addr=n; }
    os("snapb n="); oi(ncand); os(" total="); oi(tot); oc('\n'); }
  else if(starts(p,"bcmp")){ p+=4; int mode=(int)parse_num(&p); u32 k=0; // 1 changed 2 inc 3 dec
    for(u32 i=0;i<ncand;i++){ u32 w=*(volatile u8*)cand[i], o=candv[i]; int ok= mode==1? w!=o : mode==2? w>o : w<o; if(ok){ cand[k]=cand[i]; candv[k]=w; k++; } }
    ncand=k; os("bcmp n="); oi(k); oc('\n'); for(u32 i=0;i<k&&i<40;i++){ ox(cand[i]); oc(' '); oi(candv[i]); oc('\n'); } }
  else if(starts(p,"fdel2")){ p+=5; float lo=parse_f(&p), hi=parse_f(&p), lo2=parse_f(&p), hi2=parse_f(&p); u32 k=0;   // change in [lo,hi] or [lo2,hi2], value |v|<400
    for(u32 i=0;i<ncand;i++){ u32 w=*(volatile u32*)cand[i], o=candv[i]; float v=*(float*)&w, d=v-*(float*)&o; if(d<0) d=-d; if(v<0) v=-v;
      if(v<400.f && ((d>=lo&&d<=hi)||(d>=lo2&&d<=hi2))){ cand[k]=cand[i]; candv[k]=w; k++; } }
    ncand=k; os("fdel n="); oi(k); oc('\n'); for(u32 i=0;i<k&&i<30;i++){ ox(cand[i]); oc(' '); of(*(volatile float*)cand[i]); oc('\n'); } }
  else if(starts(p,"fdel")){ p+=4; float lo=parse_f(&p), hi=parse_f(&p); int isf=cand_sz&0x80; u32 k=0;   // keep candidates whose change |new-old| is in [lo,hi]
    for(u32 i=0;i<ncand;i++){ u32 w=*(volatile u32*)cand[i], o=candv[i]; float d; if(isf){ d=*(float*)&w-*(float*)&o; } else d=(float)((s32)w-(s32)o); if(d<0) d=-d;
      if(d>=lo&&d<=hi){ cand[k]=cand[i]; candv[k]=w; k++; } }
    ncand=k; os("fdel n="); oi(k); oc('\n'); for(u32 i=0;i<k&&i<30;i++){ ox(cand[i]); oc(' '); if(isf) of(*(volatile float*)cand[i]); else oi((s32)candv[i]); oc('\n'); } }
  else if(starts(p,"rsr")){ os("rs dyaw "); of(dbg_dyaw); os(" dpit "); of(dbg_dpit); os(" left "); oi(dbg_rs_n); oc('\n'); }
  else if(starts(p,"rs")){ p+=2; dbg_rs_x=(s32)(s64)parse_num(&p); dbg_rs_y=(s32)(s64)parse_num(&p); dbg_rs_n=dbg_rs_tot=(int)parse_num(&p); cam_angles(&dbg_yaw0,&dbg_pit0); dbg_dyaw=dbg_dpit=0; os("rs\n"); }
  else if(starts(p,"cw32")){ p+=4; u64 a=parse_num(&p); u32 v=(u32)parse_num(&p); if(a<0x10000000) a+=g_main;   // write one instruction into code
    u32 old=RD32(a); int ok=code_write32(a,v); os("cw32 "); ox(a); oc(' '); ox(old); os(" -> "); ox(RD32(a)); os(" ok="); oi(ok); os(" rc="); ox(g_patch_rc); oc(10); }
  else if(starts(p,"ptest")){ p+=5; int m=(int)parse_num(&p); u64 f=(u64)&pt_fn; os("before "); oi(pt_fn_call()); oc(' ');
    if(m==1){ int ok=code_write32(f,0x52800040u); os("map ok="); oi(ok); os(" rc="); ox(g_patch_rc); }
    else if(m==4){ os("self handle "); ox(self_handle()); os(" rc="); ox(g_patch_rc); }
    else if(m==2){ u64 pg=f&~0xfffull; u32 r=svcSetProcessMemoryPermission(self_handle(),pg,0x1000,3); os("perm rw rc="); ox(r);
      if(!r){ *(volatile u32*)f=0x52800060u; icache_sync(f,f,4); r=svcSetProcessMemoryPermission(CUR_PROCESS,pg,0x1000,5); os(" back rc="); ox(r); } }
    else if(m==3){ os("free va "); ox(free_va(0x2000)); }
    os(" after "); oi(pt_fn_call()); oc(10); }
  else if(starts(p,"mfile")){ p+=5; u64 a=parse_num(&p); u64 n=parse_num(&p);   // raw memory -> sd:/touchmod/mem.bin (copied through a buffer, unmapped pages skipped as zeros)
    static u8 mb[0x10000]; FsDelete("sd:/touchmod/mem.bin"); FsCreate("sd:/touchmod/mem.bin",(s64)n); FH h; if(FsOpen(&h,"sd:/touchmod/mem.bin",2)){ os("open fail"); oc(10); return; }
    WOpt o={0}; for(u64 off=0;off<n;off+=sizeof(mb)){ u64 k=n-off; if(k>sizeof(mb)) k=sizeof(mb); if(mem_ok(a+off)) for(u64 i=0;i<k;i++) mb[i]=*(volatile u8*)(a+off+i); else for(u64 i=0;i<k;i++) mb[i]=0; FsWrite(h,(s64)off,mb,k,&o); }
    WOpt f={1}; FsWrite(h,0,mb,0,&f); FsClose(h); os("mfile ok"); oc(10); }
  else if(starts(p,"csens")){ p+=5; g_cam_sens[0]=(int)parse_num(&p); g_cam_sens[1]=(int)parse_num(&p); os("csens"); oc(10); }
  else if(starts(p,"drag")){ p+=4; dg_x0=(float)(s64)parse_num(&p); dg_y0=(float)(s64)parse_num(&p); dg_x1=(float)(s64)parse_num(&p); dg_y1=(float)(s64)parse_num(&p);
    dg_n=(int)parse_num(&p); dg_hold=(int)parse_num(&p); dg_i=0; os("drag"); oc(10); }
  else if(starts(p,"recd")){ for(int i=0;i<rec_i;i++){ oi(i); for(int k=0;k<4;k++){ oc(' '); of(*(float*)&rec_v[i][k]); oc('/'); ox(rec_v[i][k]); } oc(' '); ox(rec_v[i][4]); oc(10); } }
  else if(starts(p,"rec")){ p+=3; rec_base=parse_num(&p); for(int k=0;k<4;k++) rec_off[k]=parse_num(&p); rec_n=(int)parse_num(&p); if(rec_n>REC_MAX) rec_n=REC_MAX;
    rec_tx=(int)parse_num(&p); rec_ty=(int)parse_num(&p); rec_tap= rec_tx||rec_ty; rec_i=0; os("rec"); oc(10); }
  else if(starts(p,"ang")){ float a,b; cam_angles(&a,&b); os("yaw "); of(a); os(" pit "); of(b); oc('\n'); }
  else if(starts(p,"ls")){ p+=2; dbg_ls_x=(s32)(s64)parse_num(&p); dbg_ls_y=(s32)(s64)parse_num(&p); dbg_ls_n=(int)parse_num(&p); os("ls\n"); }
  else if(starts(p,"perfreset")){ pf_n=pf_sum=pf_max=0; for(int i=0;i<6;i++) pf_hist[i]=0; for(int i=0;i<16;i++){ pf_sec[i]=0; pf_maxsec[i]=0; } g_memq=0; os("perf reset"); oc(10); }
  else if(starts(p,"perfsec")){ os("ns/poll by section:"); for(int i=0;i<15;i++){ oc(32); oi(pf_n ? (s64)(pf_sec[i]*10000/192/pf_n) : 0); } os(" | memq/poll x100 "); oi(pf_n ? (s64)(g_memq*100/pf_n) : 0); os(" | worst poll us by section:"); for(int i=0;i<15;i++){ oc(32); oi((s64)(pf_maxsec[i]*10/192)); } oc(10); }
  else if(starts(p,"mocache")){ p+=7; g_mocache=(int)parse_num(&p); os("mocache "); oi(g_mocache); oc(10); }
  else if(starts(p,"tlog")){ p+=4; tlog=(int)parse_num(&p); os("tlog "); oi(tlog); oc(10); }
  else if(starts(p,"perf")){ os("polls "); oi((s64)pf_n); os(" avg_ns "); oi(pf_n ? (s64)(pf_sum*10000/192/pf_n) : 0); os(" max_us "); oi((s64)(pf_max*10/192));
    os(" hist_us<5,<10,<20,<50,<100,more "); for(int i=0;i<6;i++){ oi((s64)pf_hist[i]); oc(32); } oc(10); }
  else if(starts(p,"polls")){ os("poll "); oi((s64)g_poll); os(" t "); oi((s64)(ticks()/19200)); oc('\n'); }
  else if(starts(p,"jmode")){ p+=5; g_jump_dir=(int)parse_num(&p); os("ok\n"); }
  else if(starts(p,"call")){ p+=4; u64 f=parse_num(&p); u64 a0=parse_num(&p),a1=parse_num(&p),a2=parse_num(&p),a3=parse_num(&p); if(f<0x10000000) f+=g_main;
    u64 r=((u64(*)(u64,u64,u64,u64))f)(a0,a1,a2,a3); os("ret "); ox(r); oc('\n'); }
  else if(starts(p,"seti")){ p+=4; u64 w=parse_num(&p); int it=(int)parse_num(&p); int a=(int)parse_num(&p); if(!mem_ok(w)){ os("bad\n"); return; } list_jump(w,it,a); os("jump\n"); }
  else if(starts(p,"pitches")){ for(int i=0;i<nlrn;i++){ ox(lrn[i].vt); oc(' '); of(lrn[i].pitch); oc(' '); of(lrn[i].xpitch); oc('\n'); } os("n="); oi(nlrn); oc('\n'); }
  else if(starts(p,"tg")){ p+=2; int x=(int)parse_num(&p), y=(int)parse_num(&p), w2=(int)parse_num(&p), st=(int)parse_num(&p); u64 b=battle(); u64 c=cell_at(x,y); if(!b||!c){ os("no\n"); return; }
    u64 u=RD64(c), ctl=RD64(b+0x3bb8); os("unit "); ox(u); os(" ctl "); ox(ctl); os(" vt "); ox(mem_ok(ctl)?RD64(ctl)-g_main:0); os(" kind "); oi(RDS32(b+0x3cdc)); os(" item "); ox(RD64(RD64(b+0x3b60)+0x140)); os(" 3b68 "); ox(RD64(b+0x3b68)); os(" 3b70 "); ox(RD64(b+0x3b70));
    if(u&&mem_ok(ctl)){ u32 r=((TgtFn)(g_main+0x181c90))(ctl,u,(u32)w2,0,(u32)st,-1,-1); os(" r="); oi(r&0xff); } oc('\n'); }
  else if(starts(p,"pmode")){ p+=5; g_pick_mode=(int)parse_num(&p); g_gh_ok=0; os("pmode\n"); }
  else if(starts(p,"gpick")){ p+=5; float x=(float)(s64)parse_num(&p), y=(float)(s64)parse_num(&p); int i=-1,j=-1,r=pick_tile_grid(x,y,&i,&j), pi=-1,pj=-1; pick_tile_plane(x,y,&pi,&pj,0,0);
    os("grid "); oi(r); oc(' '); oi(i); oc(','); oi(j); os(" plane "); oi(pi); oc(','); oi(pj); oc('\n'); }
  else if(starts(p,"hc")){ p+=2; int x0=(int)parse_num(&p), y0=(int)parse_num(&p), x1=(int)parse_num(&p), y1=(int)parse_num(&p);
    for(int j=y0;j<=y1;j++){ for(int i=x0;i<=x1;i++){ u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(po&&pz){ oi((s64)tile_height(RDF(po)+(float)i*1000.f,RDF(pz)+(float)j*1000.f,-1.f)); oc(' '); } } oc('\n'); } }
  else if(starts(p,"h2")){ p+=2; int i0=(int)parse_num(&p), j0=(int)parse_num(&p), i1=(int)parse_num(&p), j1=(int)parse_num(&p);   // per tile: height, both layers of main+0x5a5d30, terrain, terrain byte +0xa
    u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return;
    for(int j=j0;j<=j1;j++){ for(int i=i0;i<=i1;i++){ float pp[4]={RDF(po)+(i+0.5f)*1000.f,0.f,RDF(pz)+(j+0.5f)*1000.f,1.f}; float a=0,b=0,a1=0,b1=0;
        ((void(*)(float*,int,float*,float*))(g_main+0x5a5d30))(pp,0,&a,&b); ((void(*)(float*,int,float*,float*))(g_main+0x5a5d30))(pp,1,&a1,&b1);
        u64 ce=cell_at(i,j); int t= ce ? *(volatile u8*)(ce+((*(volatile u8*)(ce+0x24)&1)?0x2c:0x2b)) : -1;
        int fl= t>=0 ? (int)((u8(*)(int))(g_main+0x403570))(t) : -1;
        oi(i); oc(','); oi(j); oc(' '); oi((s64)tile_height(pp[0],pp[2],-1.f)); oc(' '); oi((s64)a); oc('/'); oi((s64)b); oc(' '); oi((s64)a1); oc('/'); oi((s64)b1); os(" t"); ox(t); oc(' '); oi(fl); os(" | "); }
      oc('\n'); } }
  else if(starts(p,"hs")){ p+=2; s64 i0=(s64)parse_num(&p), j0=(s64)parse_num(&p), di=(s64)parse_num(&p)-1000, dj=(s64)parse_num(&p)-1000; int n=(int)parse_num(&p);   // heights along a line, positions in 1/100 tile from the map origin (di, dj + 1000)
    u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(!po||!pz) return; if(n>60) n=60;
    for(int k=0;k<=n;k++){ float pp[4]={RDF(po)+(float)(i0+k*di)*10.f,0.f,RDF(pz)+(float)(j0+k*dj)*10.f,1.f}; float a=0,b=0;
      ((void(*)(float*,int,float*,float*))(g_main+0x5a5d30))(pp,0,&a,&b); oi((s64)(i0+k*di)); oc(','); oi((s64)(j0+k*dj)); oc(' '); oi((s64)a); oc('/'); oi((s64)b); oc(' '); oi((s64)tile_height(pp[0],pp[2],-1.f)); oc(10); } }
  else if(starts(p,"cells")){ p+=5; int x0=(int)parse_num(&p), y0=(int)parse_num(&p), x1=(int)parse_num(&p), y1=(int)parse_num(&p);
    for(int j=y0;j<=y1;j++){ for(int i=x0;i<=x1;i++){ u64 po=RD64(g_main+0x1aa8198), pz=RD64(g_main+0x1aa81a0); if(po&&pz){ oi((s64)tile_height(RDF(po)+(i+0.5f)*1000.f,RDF(pz)+(j+0.5f)*1000.f,-1.f)); oc(' '); } } oc('\n'); } }
  else if(starts(p,"sstr")){ p+=4; while(*p==' ') p++; char t[64]; int n=0; while(*p&&*p!='\n'&&*p!=';'&&n<63){ t[n++]=*p++; } t[n]=0; if(n<2){ os("short\n"); return; }
    // search all rw regions within the range set by `range lo hi` for the UTF-8 text
    u64 addr=0; MemInfo mi; u32 pi; int hits=0;
    while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(is_rw(&mi) && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo) && mi.type!=0xB && mi.type!=0xC){
        u64 b0=mi.addr<sr_lo?sr_lo:mi.addr; u64 e=mi.addr+mi.size; if(e>sr_hi) e=sr_hi; if(e>b0+n) for(u64 a=b0;a<e-n;a++){ if(*(volatile char*)a!=t[0]) continue; int k=1; while(k<n && *(volatile char*)(a+k)==t[k]) k++; if(k==n){ if(hits<60){ ox(a); oc('\n'); } hits++; } } }
      u64 nx=mi.addr+mi.size; if(nx<=addr||!nx) break; addr=nx; }
    os("hits="); oi(hits); oc('\n'); }
  else os("?\n");
}
#endif
static int inited; static u64 calls;
static TouchScreenState1 ts; static TouchScreenState1 tsh[16]; static s64 last_samp=-1; static u64 cur_inject;

#ifdef TM_DEBUG
#define PF(i,x) { u64 a_=ticks(); x; u64 d_=ticks()-a_; pf_sec[i]+=d_; pf_cur[i]+=d_; }
#else
#define PF(i,x) { x; }
#endif
static void process_(NpadState*s,u32 id,u64 lr);
static void process(NpadState*s,u32 id,u64 lr){
#ifdef TM_DEBUG
  for(int i=0;i<16;i++) pf_cur[i]=0;
  u64 t0=ticks(); pf_dbg=0; process_(s,id,lr); u64 dt=ticks()-t0-pf_dbg; pf_n++; pf_sum+=dt; if(dt>pf_max){ pf_max=dt; for(int i=0;i<16;i++) pf_maxsec[i]=pf_cur[i]; }
  { u64 us=dt*10/192; int k= us<5?0: us<10?1: us<20?2: us<50?3: us<100?4: 5; pf_hist[k]++; }
#else
  process_(s,id,lr);
#endif
}
static void process_(NpadState*s,u32 id,u64 lr){
  if(!InitTouch||!GetTouch1) return;
  if(!inited){ inited=1; InitTouch(); find_main(lr); Buf b={0}; bstr(&b,"[touchmod] init main="); bhex(&b,g_main); bflush(&b); }
  calls++;
  if(g_primary_id==0xffffffff) g_primary_id=id;
  if(id==g_primary_id){
    g_poll++;
#ifdef TM_DEBUG
    if((g_poll%10)==0){ u64 c0=ticks(); poll_cmd(); pf_dbg+=ticks()-c0; }
    if(dbg_tgt){ dbg_tgt=0; int r=target_select(dbg_tgt_x,dbg_tgt_y); Buf bb={0}; bstr(&bb,"[touchmod] tgt r="); bint(&bb,r); bflush(&bb); }
    if(dbg_tt){ dbg_tt=0; g_force_tile=1; g_ftx=dbg_tt_x; g_fty=dbg_tt_y; on_tap(600.f,300.f); g_force_tile=0; }
    if(dbg_tap){ dbg_tap=0; on_tap((float)dbg_tap_x,(float)dbg_tap_y); }
    if(inj_frames>0){ inj_frames--; cur_inject_dbg=inj_buttons; } else cur_inject_dbg=0;
    if(mash_n>0){ mash_n--; if((mash_i++%mash_period)<2) cur_inject_dbg|=mash_mask; }
#endif
#ifdef TM_DEBUG
    if(!drag_poll())
#endif
    PF(0, if(GetTouchN){ int n=GetTouchN(tsh,16); if(n>16) n=16;
      for(int i=n-1;i>=0;i--){ if(tsh[i].sampling>last_samp){ last_samp=tsh[i].sampling; PF(1, touch_update(&tsh[i])); } } }
    else { GetTouch1(&ts); PF(1, touch_update(&ts)); })
    PF(2, chase_tick(); tcycle_tick());
    PF(3, rt_tick(); ab_tick(); fz_tick(); tr_tick(); cert_tick());
    PF(4, hc_tick());
    PF(5, prep_tick());
    PF(6, cursor_hide_tick(); cursor_pin_tick());
    PF(7, learn_tick());
    PF(8, own_track());
    PF(9, if(log_arm){ u64 g=cmn_ui(); if(!g || !*(volatile u8*)(g+0x50) || RD64(g+0x58)!=log_own) log_arm=0; })   // the Log closed: drop the armed entry
    PF(10, wa_tick());
    PF(11, dt_tick());
    PF(13, cur_inject=q_tick()|hold_tick());
    PF(14, cam_tick());
    PF(12, opts_tick());
  }
  if(g_rs_x||g_rs_y){ s->r.x=g_rs_x; s->r.y=g_rs_y; }
#ifdef TM_DEBUG
  if(dbg_ls_n>0){ s->l.x=dbg_ls_x; s->l.y=dbg_ls_y; if(id==g_primary_id) dbg_ls_n--; }
  if(dbg_rs_n>0){ s->r.x=dbg_rs_x; s->r.y=dbg_rs_y; if(id==g_primary_id){ float a,b; cam_angles(&a,&b); float d=a-dbg_yaw0; if(d>3.14159f) d-=6.28318f; if(d<-3.14159f) d+=6.28318f; dbg_dyaw+=d; dbg_yaw0=a; dbg_dpit+=b-dbg_pit0; dbg_pit0=b; dbg_rs_n--; } }
#endif
#ifdef TM_DEBUG
  if(id==g_primary_id) rec_poll(cur_inject|cur_inject_dbg);
#endif
  if(s->buttons&0xffffffu) g_touch_mode=0; // real controller/keyboard input brings the cursor back
  s->buttons|=cur_inject;
#ifdef TM_DEBUG
  s->buttons|=cur_inject_dbg;
#endif
  // Options -> TouchMod row: left / right change the mod's option and are hidden from the game
  if(id==g_primary_id) opts_buttons(s->buttons);
  if(g_opt_mask){ s->buttons&=~g_opt_mask; s->l.x=0; s->r.x=0; }
}
#define HOOK(kind,Fn) { if(!Fn){ memset(s,0,sizeof(*s)); return; } Fn(s,1,id); process(s,*id,(u64)__builtin_return_address(0)); }
void _ZN2nn3hid12GetNpadStateEPNS0_16NpadFullKeyStateERKj(NpadState*s,const u32*id) HOOK(0,GetStatesFull)
void _ZN2nn3hid12GetNpadStateEPNS0_17NpadHandheldStateERKj(NpadState*s,const u32*id) HOOK(1,GetStatesHand)
void _ZN2nn3hid12GetNpadStateEPNS0_16NpadJoyDualStateERKj(NpadState*s,const u32*id) HOOK(2,GetStatesDual)
