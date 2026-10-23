// ---- writing to the game's code (hooks) ----
// Needs the memory SVCs in main.npdm. The code pages stay R-X; the bytes are written through a temporary RW mapping of the
// same physical pages (svcMapProcessMemory on our own process), then the instruction cache is invalidated (Ryujinx drops the
// recompiled code for the line on `ic ivau`).
#define CUR_PROCESS 0xFFFF8001u
static u32 g_patch_rc;   // last failure (svc result, high byte = step), 0 = ok
static inline u32 svc4(u32 n,u64 a,u64 b,u64 c,u64 d){
  register u64 x0 asm("x0")=a; register u64 x1 asm("x1")=b; register u64 x2 asm("x2")=c; register u64 x3 asm("x3")=d;
  switch(n){
    case 0x73: asm volatile("svc 0x73":"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x3)::"x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); break;
    case 0x74: asm volatile("svc 0x74":"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x3)::"x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); break;
    case 0x75: asm volatile("svc 0x75":"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x3)::"x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); break;
    default: return 0xffffffffu; }
  return (u32)x0; }
// generic SVC: 6 inputs, returns x0, x1 in *o1
#define SVC6(N) static inline u32 svc_##N(u64 a,u64 b,u64 c,u64 d,u64 e,u64 f,u64*o1){   register u64 x0 asm("x0")=a; register u64 x1 asm("x1")=b; register u64 x2 asm("x2")=c; register u64 x3 asm("x3")=d; register u64 x4 asm("x4")=e; register u64 x5 asm("x5")=f;   asm volatile("svc " #N:"+r"(x0),"+r"(x1),"+r"(x2),"+r"(x3),"+r"(x4),"+r"(x5)::"x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory");   if(o1) *o1=x1; return (u32)x0; }
SVC6(0x08) SVC6(0x09) SVC6(0x0a) SVC6(0x16) SVC6(0x18) SVC6(0x21) SVC6(0x40) SVC6(0x43)
static inline u32*tls_ptr(void){ u64 t; asm volatile("mrs %0, tpidrro_el0":"=r"(t)); return (u32*)t; }

// A real handle to our own process: send CUR_PROCESS in an IPC message to ourselves (a copied handle comes out of the kernel as
// a real one), the way libnx / exlaunch do it. Needs CreateSession (0x40) and ReplyAndReceive (0x43) in main.npdm.
static u32 g_self_handle;
static u8 g_ph_stack[0x2000] __attribute__((aligned(0x1000)));
static void ph_thread(u64 server){
  u32 h=(u32)server; u64 idx=0;
  if(svc_0x43(0,(u64)&h,1,0,~0ull,0,&idx)==0) g_self_handle=tls_ptr()[3];
  svc_0x16(h,0,0,0,0,0,0);    // close the server side: the client's request returns (with an error, ignored)
  svc_0x0a(0,0,0,0,0,0,0); }  // ExitThread
static u32 self_handle(void){
  if(g_self_handle) return g_self_handle;
  u64 server=0, client=0, th=0; u32 r;
  { register u64 x0 asm("x0"); register u64 x1 asm("x1"); register u64 x2 asm("x2")=0; register u64 x3 asm("x3")=0;   // CreateSession(&server,&client,0,0)
    asm volatile("svc 0x40":"=r"(x0),"=r"(x1),"+r"(x2),"+r"(x3)::"x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory");
    r=(u32)x0; server=x1; client=x2; }
  if(r){ g_patch_rc=0x40000000u|r; return 0; }
  r=svc_0x08(0,(u64)ph_thread,server,(u64)(g_ph_stack+sizeof(g_ph_stack)),44,(u64)-2,&th);   // CreateThread(&h, entry, arg, stack top, prio, default core)
  if(r){ g_patch_rc=0x08000000u|r; svc_0x16(server,0,0,0,0,0,0); svc_0x16(client,0,0,0,0,0,0); return 0; }
  svc_0x09(th,0,0,0,0,0,0);
  u32*m=tls_ptr(); m[0]=0; m[1]=0x80000000u; m[2]=2; m[3]=CUR_PROCESS;   // header (no data), special header: copy 1 handle
  svc_0x21(client,0,0,0,0,0,0);
  u32 hs=(u32)th; u64 idx; svc_0x18(0,(u64)&hs,1,~0ull,0,0,&idx);      // wait for the thread
  svc_0x16(th,0,0,0,0,0,0); svc_0x16(client,0,0,0,0,0,0);
  if(!g_self_handle) g_patch_rc=0x43000000u;
  return g_self_handle; }

#define svcSetProcessMemoryPermission(h,a,s,p) svc4(0x73,(h),(a),(s),(p))
#define svcMapProcessMemory(dst,h,src,s) svc4(0x74,(dst),(h),(src),(s))
#define svcUnmapProcessMemory(dst,h,src,s) svc4(0x75,(dst),(h),(src),(s))

static void icache_sync(u64 rw,u64 rx,u64 n){
  for(u64 i=0;i<n;i+=4){ asm volatile("dc cvau, %0"::"r"(rw+i):"memory"); }
  asm volatile("dsb ish":::"memory");
  for(u64 i=0;i<n;i+=4){ asm volatile("ic ivau, %0"::"r"(rx+i):"memory"); }
  asm volatile("dsb ish\n isb":::"memory"); }

SVC6(0x29)
static u64 proc_info(u32 type){ u64 v=0; svc_0x29(0,type,CUR_PROCESS,0,0,0,&v); return v; }   // svcGetInfo
// An unmapped range for the temporary RW view: inside the ASLR region but outside the heap and alias regions (the kernel refuses
// those for svcMapProcessMemory), as libnx's virtmemFindCodeMemory does; 1 MB of guard on both sides.
static u64 free_va(u64 size){
  u64 as=proc_info(12), ae=as+proc_info(13), hs=proc_info(4), he=hs+proc_info(5), ls=proc_info(2), le=ls+proc_info(3);
  if(!as||ae<=as){ as=0x8000000; ae=1ull<<39; }
  u64 guard=0x100000, need=size+2*guard, addr=as; MemInfo mi; u32 pi;
  while(addr<ae){ if(svcQueryMemory(&mi,&pi,addr)) break;
    u64 s=mi.addr<as?as:mi.addr, e=mi.addr+mi.size; if(e>ae||e<mi.addr) e=ae;
    if(mi.type==0){   // free: slide the candidate past the heap / alias regions while it overlaps one
      u64 c=s; for(int moved=1;moved && c+need<=e;){ moved=0;
        if(he>hs && c<he && hs<c+need){ c=he; moved=1; }
        if(le>ls && c<le && ls<c+need){ c=le; moved=1; } }
      if(c+need<=e) return c+guard; }
    u64 nx=mi.addr+mi.size; if(nx<=addr||!nx) break; addr=nx; }
  return 0; }

// Write n bytes (n <= 0x1000 - page offset is handled by mapping 2 pages) at code address dst.
static int code_write(u64 dst,const void*src,u64 n){
  u64 pg=dst&~0xfffull, len=((dst+n+0xfff)&~0xfffull)-pg;
  u32 ph=self_handle(); if(!ph) return 0;
  u64 va=free_va(len); if(!va){ g_patch_rc=1; return 0; }
  u32 r=svcMapProcessMemory(va,ph,pg,len); if(r){ g_patch_rc=r; return 0; }
  volatile u8*w=(volatile u8*)(va+(dst-pg)); const u8*s=(const u8*)src; for(u64 i=0;i<n;i++) w[i]=s[i];
  icache_sync(va+(dst-pg),dst,n);
  r=svcUnmapProcessMemory(va,ph,pg,len); if(r){ g_patch_rc=0x80000000u|r; return 0; }
  g_patch_rc=0; return 1; }
static int code_write32(u64 dst,u32 insn){ return code_write(dst,&insn,4); }
// `bl`/`b` from `at` to `to` (+-128 MB)
static u32 a64_branch(u64 at,u64 to,int link){ s64 d=(s64)(to-at)>>2; return (link?0x94000000u:0x14000000u)|((u32)d&0x3ffffff); }
static int a64_in_range(u64 at,u64 to){ s64 d=(s64)(to-at); return d>=-(128ll<<20) && d<(128ll<<20); }
