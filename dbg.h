static u64 g_main;
// debug command channel via sd card
typedef struct { void* h; } FH;
typedef struct { int flags; } WOpt;
IMPORT(u32,_ZN2nn2fs19MountSdCardForDebugEPKc,FsMountSd,const char*)
IMPORT(u32,_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci,FsOpen,FH*,const char*,int)
IMPORT(u32,_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE,FsSize,s64*,FH)
IMPORT(u32,_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvmRKNS0_10ReadOptionE,FsReadN,u64*,FH,s64,void*,u64,const WOpt*)   // the overload with the byte count: a short read (the host rewriting cmd.txt) is not an abort
IMPORT(u32,_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE,FsWrite,FH,s64,const void*,u64,const WOpt*)
IMPORT(void,_ZN2nn2fs9CloseFileENS0_10FileHandleE,FsClose,FH)
IMPORT(u32,_ZN2nn2fs10CreateFileEPKcl,FsCreate,const char*,s64)
IMPORT(u32,_ZN2nn2fs10DeleteFileEPKc,FsDelete,const char*)
#define FsMountSd FsMountSd_p()
#define FsOpen FsOpen_p()
#define FsSize FsSize_p()
#define FsReadN FsReadN_p()
#define FsWrite FsWrite_p()
#define FsClose FsClose_p()
#define FsCreate FsCreate_p()
#define FsDelete FsDelete_p()

typedef struct { u64 addr, size; u32 type, attr, perm, ipc, dev, pad; } MemInfo;
static inline u32 svcQueryMemory(MemInfo* mi, u32* pi, u64 addr){ register u64 x0 asm("x0")=(u64)mi; register u64 x1 asm("x1")=0; register u64 x2 asm("x2")=addr; asm volatile("svc 0x6":"+r"(x0),"+r"(x1),"+r"(x2)::"x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); *pi=(u32)x1; return (u32)x0; }

#define MAXC (4u<<20)
static u64 cand[MAXC]; static u32 candv[MAXC]; static u32 ncand; static u32 cand_sz;
static char obuf[1<<16]; static int olen;
static void oc(char c){ if(olen<(int)sizeof(obuf)-1) obuf[olen++]=c; }
static void os(const char*s){ while(*s) oc(*s++); }
static void oi(s64 v){ char t[24]; int i=0; if(v<0){oc('-'); v=-v;} do{t[i++]='0'+v%10; v/=10;}while(v); while(i) oc(t[--i]); }
static void ox(u64 v){ const char*h="0123456789abcdef"; os("0x"); int st=0; for(int i=60;i>=0;i-=4){int d=(v>>i)&15; if(d||st||i==0){st=1;oc(h[d]);}} }
static void of(float f){ if(f!=f){os("nan");return;} if(f<0){oc('-');f=-f;} if(f>1e9f){os("big");return;} s64 ip=(s64)f; oi(ip); oc('.'); s64 fr=(s64)((f-ip)*10000); for(int d=1000;d>fr&&d>1;d/=10) oc('0'); oi(fr); }
static int mounted; static u32 mount_rc=1; static int lastseq=-1;
static u64 mod_lo, mod_hi;
extern char __nx_mod0[];
static void out_flush(void){
  FsDelete("sd:/touchmod/out.txt"); FsCreate("sd:/touchmod/out.txt", olen);
  FH h; if(FsOpen(&h,"sd:/touchmod/out.txt",2)==0){ WOpt o={1}; FsWrite(h,0,obuf,olen,&o); FsClose(h);} olen=0;
}
static u64 parse_num(const char**p){ const char*s=*p; while(*s==' ') s++; int neg=0; if(*s=='-'){neg=1;s++;} u64 v=0; if(s[0]=='0'&&s[1]=='x'){ s+=2; for(;;){char c=*s; int d; if(c>='0'&&c<='9')d=c-'0'; else if(c>='a'&&c<='f')d=c-'a'+10; else break; v=v*16+d; s++;} } else { while(*s>='0'&&*s<='9'){ v=v*10+(*s-'0'); s++; } } *p=s; return neg?(u64)(-(s64)v):v; }
static int is_rw(MemInfo*mi){ return (mi->perm&3)==3 && mi->type!=0 && mi->size>0; }
static u64 rdv(u64 a,u32 sz){ if(sz==1) return *(volatile u8*)a; if(sz==2) return *(volatile u16*)a; if(sz==4) return *(volatile u32*)a; return *(volatile u64*)a; }
static int fmatch(float f, float t){ float d=f-t; if(d<0)d=-d; return d<0.01f; }
static void do_scan(u64 v,u32 sz,int isf){
  ncand=0; cand_sz=isf?0x80|4:sz; u64 addr=0; MemInfo mi; u32 pi; int regions=0; u64 bytes=0;
  float fv; { u32 t=(u32)v; fv=*(float*)&t; }
  while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(is_rw(&mi) && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo) && mi.type!=0xB && mi.type!=0xC){ regions++; bytes+=mi.size;
      u64 e=mi.addr+mi.size;
      if(isf){ for(u64 a=mi.addr;a<e;a+=4){ float f=*(volatile float*)a; if(fmatch(f,fv)){ if(ncand<MAXC) cand[ncand++]=a; } } }
      else if(sz==4){ u32 w=(u32)v; for(u64 a=mi.addr;a<e;a+=4){ if(*(volatile u32*)a==w){ if(ncand<MAXC) cand[ncand++]=a; } } }
      else if(sz==2){ u16 w=(u16)v; for(u64 a=mi.addr;a<e;a+=2){ if(*(volatile u16*)a==w){ if(ncand<MAXC) cand[ncand++]=a; } } }
      else { u8 w=(u8)v; for(u64 a=mi.addr;a<e;a++){ if(*(volatile u8*)a==w){ if(ncand<MAXC) cand[ncand++]=a; } } }
    }
    u64 n=mi.addr+mi.size; if(n<=addr) break; addr=n; if(addr==0) break; }
  os("scan regions="); oi(regions); os(" bytes="); ox(bytes); os(" n="); oi(ncand); oc('\n');
}
static void do_filt(u64 v, int mode){ // mode 0 eq, 1 changed-from-snapshot not supported; 2 ne
  u32 sz=cand_sz&0x7f; int isf=cand_sz&0x80; u32 k=0; float fv; { u32 t=(u32)v; fv=*(float*)&t; }
  for(u32 i=0;i<ncand;i++){ u64 a=cand[i]; int ok;
    if(isf){ float f=*(volatile float*)a; ok=fmatch(f,fv); } else { u64 x=rdv(a,sz); u64 m= sz==8?~0ul:((1ul<<(sz*8))-1); ok=(x==(v&m)); }
    if(mode==2) ok=!ok; if(ok) cand[k++]=a; }
  ncand=k; os("filt n="); oi(ncand); oc('\n');
}
static void do_list(u32 n){ u32 sz=cand_sz&0x7f; int isf=cand_sz&0x80; for(u32 i=0;i<ncand&&i<n;i++){ ox(cand[i]); oc(' '); if(isf) of(*(volatile float*)cand[i]); else ox(rdv(cand[i],sz)); oc('\n'); } }
static int mem_ok(u64 a);
static void do_dump(u64 a,u32 n){ for(u32 i=0;i<n;i+=16){ if(!mem_ok(a+i)||!mem_ok(a+i+15)){ os("bad addr\n"); return; } ox(a+i); os(": "); for(u32 j=0;j<16;j+=4){ ox(*(volatile u32*)(a+i+j)); oc(' ');} os(" | "); for(u32 j=0;j<16;j+=4){ of(*(volatile float*)(a+i+j)); oc(' ');} oc('\n'); } }
static void do_regions(void){ u64 addr=0; MemInfo mi; u32 pi; while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(mi.type){ ox(mi.addr); oc(' '); ox(mi.size); os(" t="); ox(mi.type); os(" p="); oi(mi.perm); oc('\n'); } u64 n=mi.addr+mi.size; if(n<=addr) break; addr=n; if(!addr) break; } }

static u64 sr_lo=0, sr_hi=~0ul;
static void do_snap(u32 lo,u32 hi,int isf,float flo,float fhi){
  ncand=0; cand_sz=4|(isf?0x80:0); u64 addr=0; MemInfo mi; u32 pi; u64 tot=0;
  while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(is_rw(&mi) && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo) && mi.type!=0xB && mi.type!=0xC){
      u64 b0=mi.addr<sr_lo?sr_lo:mi.addr; u64 e=mi.addr+mi.size; if(e>sr_hi) e=sr_hi; for(u64 a=b0;a<e;a+=4){ u32 w=*(volatile u32*)a; int ok; if(isf){ float f=*(float*)&w; ok=(f>=flo&&f<=fhi); } else ok=(w>=lo&&w<=hi); if(ok){ tot++; if(ncand<MAXC){ cand[ncand]=a; candv[ncand]=w; ncand++; } } } }
    u64 n=mi.addr+mi.size; if(n<=addr||!n) break; addr=n; }
  os("snap n="); oi(ncand); os(" total="); oi(tot); oc('\n');
}
static void do_cmp(int mode){ // 0 same 1 changed 2 inc 3 dec
  int isf=cand_sz&0x80; u32 k=0;
  for(u32 i=0;i<ncand;i++){ u32 w=*(volatile u32*)cand[i]; u32 o=candv[i]; int ok;
    if(isf){ float f=*(float*)&w, g=*(float*)&o; ok = mode==0? (f==g) : mode==1? (f!=g) : mode==2? (f>g):(f<g); }
    else { s32 f=(s32)w,g=(s32)o; ok = mode==0? (f==g) : mode==1? (f!=g) : mode==2? (f>g):(f<g); }
    if(ok){ cand[k]=cand[i]; candv[k]=w; k++; } }
  ncand=k; os("cmp n="); oi(ncand); oc('\n');
}
static int fz(float f){ return f>-1e-5f && f<1e-5f; }
static float pr_lo=1.76f, pr_hi=1.79f;
static void do_projscan(void){
  u64 addr=0; MemInfo mi; u32 pi; int hits=0;
  while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(is_rw(&mi) && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo) && mi.type!=0xB && mi.type!=0xC){
      u64 e=mi.addr+mi.size-64; for(u64 a=mi.addr;a<e;a+=16){ volatile float*m=(volatile float*)a; float m0=m[0]; if(!(m0>0.2f&&m0<6.f)) continue; float m5=m[5]; float r=m5/m0; if(!(r>pr_lo&&r<pr_hi)) continue;
          if(!(fz(m[1])&&fz(m[3])&&fz(m[4])&&fz(m[7]))) continue;
          if(hits<200){ ox(a); os(" :"); for(int j=0;j<16;j++){ oc(' '); of(m[j]); } oc('\n'); } hits++; } }
    u64 n=mi.addr+mi.size; if(n<=addr||!n) break; addr=n; }
  os("proj hits="); oi(hits); oc('\n');
}
static float parse_f(const char**pp){ const char*p=*pp; while(*p==' ')p++; int neg=0; if(*p=='-'){neg=1;p++;} float f=0; while(*p>='0'&&*p<='9'){f=f*10+(*p-'0');p++;} if(*p=='.'){p++; float m=0.1f; while(*p>='0'&&*p<='9'){f+=m*(*p-'0'); m*=0.1f; p++;}} *pp=p; return neg?-f:f; }
static void do_listv(u32 n){ int isf=cand_sz&0x80; for(u32 i=0;i<ncand&&i<n;i++){ ox(cand[i]); oc(' '); u32 w=*(volatile u32*)cand[i]; if(isf) of(*(float*)&w); else oi((s32)w); oc('\n'); } }


static u8 msnap[0x80000]; static u64 msnap_a; static u32 msnap_n;
static void do_msnap(u64 a,u32 n){ if(n>sizeof(msnap)) n=sizeof(msnap); msnap_a=a; msnap_n=n; for(u32 i=0;i<n;i++) msnap[i]=*(volatile u8*)(a+i); os("msnap ok\n"); }
static void do_mdiff(int upd){ int c=0; for(u32 i=0;i<msnap_n;i+=4){ u32 o=*(u32*)(msnap+i); u32 w=*(volatile u32*)(msnap_a+i); if(o!=w){ if(c<300){ os("+"); ox(i); os(": "); ox(o); os(" -> "); ox(w); os("  f="); of(*(float*)&w); oc('\n'); } c++; if(upd) *(u32*)(msnap+i)=w; } } os("diffs="); oi(c); oc('\n'); }

// injected input
static int dbg_tt, dbg_tt_x, dbg_tt_y;
static int dbg_tap, dbg_tap_x, dbg_tap_y, dbg_tgt, dbg_tgt_x, dbg_tgt_y;
static u64 inj_buttons; static int inj_frames; static u64 cur_inject_dbg; static u64 mash_mask; static int mash_n, mash_period, mash_i;
extern u64 g_frame_dummy;
static void do_cmd_line(const char*p);
static void dbg_ext(const char*p);
static char cbuf[4096];
static void poll_cmd(void){
  if(!mounted){ mounted=1; u32 r=FsMountSd("sd"); mount_rc=r; Buf b={0}; bstr(&b,"[touchmod] mount sd r="); bhex(&b,r); bflush(&b); mod_lo=(u64)__nx_mod0-0x10000; mod_hi=(u64)__nx_mod0+0x10000000; }
  if(mount_rc) return;
  FH h; if(FsOpen(&h,"sd:/touchmod/cmd.txt",1)) return; s64 sz=0; FsSize(&sz,h); if(sz>4095) sz=4095; u64 got=0; WOpt ro={0}; if(sz>0 && FsReadN(&got,h,0,cbuf,(u64)sz,&ro)) got=0; FsClose(h); if(got>(u64)sz) got=0; cbuf[got]=0;
  const char*p=cbuf; int seq=(int)parse_num(&p); if(seq==lastseq) return; lastseq=seq;
  olen=0; os("seq "); oi(seq); oc('\n');
  while(*p){ while(*p=='\n'||*p=='\r'||*p==' '||*p==';') p++; if(!*p) break; do_cmd_line(p); while(*p && *p!='\n' && *p!=';') p++; }
  out_flush();
}
static int starts(const char*p,const char*w){ while(*w){ if(*p!=*w) return 0; p++; w++; } return 1; }
static u64 watch_a[16]; static u32 watch_sz[16]; static int nwatch;
static void do_cmd_line(const char*p){
  if(starts(p,"scan") && !starts(p,"scan64")){ p+=4; u32 sz=4; int isf=0; if(*p=='f'){isf=1;p++;} else if(*p=='8'){sz=1;p++;} else if(*p=='1'){sz=2;p+=2;} else if(*p=='3'){sz=4;p+=2;} else if(*p=='6'){sz=8;p+=2;}
    u64 v; if(isf){ // parse float like 12.5
      while(*p==' ')p++; int neg=0; if(*p=='-'){neg=1;p++;} float f=0; while(*p>='0'&&*p<='9'){f=f*10+(*p-'0');p++;} if(*p=='.'){p++; float m=0.1f; while(*p>='0'&&*p<='9'){f+=m*(*p-'0'); m*=0.1f; p++;}} if(neg)f=-f; u32 t=*(u32*)&f; v=t; }
    else v=parse_num(&p); do_scan(v,sz,isf); }
  else if(starts(p,"filtf")){ p+=5; while(*p==' ')p++; int neg=0; if(*p=='-'){neg=1;p++;} float f=0; while(*p>='0'&&*p<='9'){f=f*10+(*p-'0');p++;} if(*p=='.'){p++; float m=0.1f; while(*p>='0'&&*p<='9'){f+=m*(*p-'0'); m*=0.1f; p++;}} if(neg)f=-f; u32 t=*(u32*)&f; do_filt(t,0); }
  else if(starts(p,"filtne")){ p+=6; do_filt(parse_num(&p),2); }
  else if(starts(p,"filt")){ p+=4; do_filt(parse_num(&p),0); }
  else if(starts(p,"list")){ p+=4; do_list((u32)parse_num(&p)); }
  else if(starts(p,"dump")){ p+=4; u64 a=parse_num(&p); u32 n=(u32)parse_num(&p); do_dump(a,n); }
  else if(starts(p,"base")){ u64 b=*(volatile u64*)(g_main+0x1aa77a8); u64 h=*(volatile u64*)(g_main+0x1aca288); u64 g=*(volatile u64*)(g_main+0x21d9ae0); os("main="); ox(g_main); os(" battle="); ox(b); os(" hud="); ox(h); os(" G="); ox(g); oc('\n'); if(b&&mem_ok(b)){ os("cur="); oi(*(volatile s32*)(b+4)); oc(','); oi(*(volatile s32*)(b+8)); os(" phase="); oi(*(volatile s32*)(b+0x3cc0)); oc('\n'); } }
  else if(starts(p,"wl")){ p+=2; u64 base=parse_num(&p); u64 off=parse_num(&p); u64 w=base+off; if(!mem_ok(w)){ os("bad\n"); } else { os("vt=+"); ox(*(volatile u64*)w-g_main); os(" vis="); oi(*(volatile u8*)(w+0x21)); os(" idx="); oi(*(volatile s32*)(w+0x4c)); os(" cnt="); oi(*(volatile s32*)(w+0x54)); oc('\n'); } }
  else if(starts(p,"w32")){ p+=3; u64 a=parse_num(&p); u64 v=parse_num(&p); if(!mem_ok(a)) os("bad\n"); else { *(volatile u32*)a=(u32)v; os("ok\n"); } }
  else if(starts(p,"w8")){ p+=2; u64 a=parse_num(&p); u64 v=parse_num(&p); if(!mem_ok(a)) os("bad\n"); else { *(volatile u8*)a=(u8)v; os("ok\n"); } }
  else if(starts(p,"regions")){ do_regions(); }
  else if(starts(p,"btn")){ p+=3; inj_buttons=parse_num(&p); inj_frames=(int)parse_num(&p); os("inj\n"); }
  else if(starts(p,"range")){ p+=5; sr_lo=parse_num(&p); sr_hi=parse_num(&p); if(!sr_hi) sr_hi=~0ul; os("range ok\n"); }
  else if(starts(p,"snapf")){ p+=5; float a=parse_f(&p), b=parse_f(&p); do_snap(0,0,1,a,b); }
  else if(starts(p,"snap")){ p+=4; u32 a=(u32)parse_num(&p), b=(u32)parse_num(&p); do_snap(a,b,0,0,0); }
  else if(starts(p,"same")){ do_cmp(0); }
  else if(starts(p,"chg")){ do_cmp(1); }
  else if(starts(p,"inc")){ do_cmp(2); }
  else if(starts(p,"dec")){ do_cmp(3); }
  else if(starts(p,"lv")){ p+=2; do_listv((u32)parse_num(&p)); }
  else if(starts(p,"proj")){ p+=4; float a=parse_f(&p), b=parse_f(&p); if(a>0){pr_lo=a; pr_hi=b;} do_projscan(); }
  else if(starts(p,"mash")){ p+=4; mash_mask=parse_num(&p); mash_n=(int)parse_num(&p); mash_period=(int)parse_num(&p); if(mash_period<2) mash_period=8; mash_i=0; os("mash\n"); }
  else if(starts(p,"msnap")){ p+=5; u64 a=parse_num(&p); u32 n=(u32)parse_num(&p); do_msnap(a,n); }
  else if(starts(p,"mdiffu")){ do_mdiff(1); }
  else if(starts(p,"mdiff")){ do_mdiff(0); }
  else if(starts(p,"scan64")){ p+=6; u64 v=parse_num(&p); ncand=0; cand_sz=8; u64 addr=0; MemInfo mi; u32 pi; while(1){ if(svcQueryMemory(&mi,&pi,addr)) break; if(is_rw(&mi) && !(mi.addr<mod_hi && mi.addr+mi.size>mod_lo) && mi.type!=0xB && mi.type!=0xC){ u64 e=mi.addr+mi.size; for(u64 a=mi.addr;a<e;a+=8){ if(*(volatile u64*)a==v && ncand<MAXC) cand[ncand++]=a; } } u64 n=mi.addr+mi.size; if(n<=addr||!n) break; addr=n; } os("scan64 n="); oi(ncand); oc('\n'); }
  else if(starts(p,"units")){ u64 g=*(volatile u64*)(g_main+0x1aa7cc8); u64 base=g?*(volatile u64*)g:0; if(base) for(int y=0;y<32;y++) for(int x=0;x<32;x++){ u64 c=base+(u64)y*0x600+(u64)x*0x30; u64 u=*(volatile u64*)c; if(u){ oi(x); oc(','); oi(y); os(" f="); oi(*(volatile u8*)(u+0xf1)); os(" flag26="); ox(*(volatile u8*)(c+0x26)); oc('\n'); } } }
  else if(starts(p,"tt")){ p+=2; u64 x=parse_num(&p), y=parse_num(&p); dbg_tt_x=(int)x; dbg_tt_y=(int)y; dbg_tt=1; os("tt queued\n"); }
  else if(starts(p,"tgt")){ p+=3; u64 x=parse_num(&p), y=parse_num(&p); dbg_tgt_x=(int)x; dbg_tgt_y=(int)y; dbg_tgt=1; os("tgt queued\n"); }
  else if(starts(p,"tap")){ p+=3; u64 x=parse_num(&p), y=parse_num(&p); dbg_tap_x=(int)x; dbg_tap_y=(int)y; dbg_tap=1; os("tap queued\n"); }
  else if(starts(p,"info")){ os("mod0="); ox((u64)__nx_mod0); oc('\n'); }
  else { dbg_ext(p); }
}
