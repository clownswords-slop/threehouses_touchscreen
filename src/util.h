#pragma once
typedef unsigned long u64; typedef long s64; typedef unsigned int u32; typedef int s32;
typedef unsigned short u16; typedef unsigned char u8;
static inline void svc_log(const char*s,u64 n){ register const char*x0 asm("x0")=s; register u64 x1 asm("x1")=n; asm volatile("svc 0x27":"+r"(x0),"+r"(x1)::"x2","x3","x4","x5","x6","x7","x8","x9","x10","x11","x12","x13","x14","x15","x16","x17","cc","memory"); }
static inline u64 ticks(void){ u64 t; asm volatile("mrs %0, cntpct_el0":"=r"(t)); return t; }
typedef struct { char b[512]; int n; } Buf;
static void bput(Buf*b,char c){ if(b->n<511) b->b[b->n++]=c; }
static void bstr(Buf*b,const char*s){ while(*s) bput(b,*s++); }
static void bint(Buf*b,s64 v){ char t[24]; int i=0; if(v<0){bput(b,'-'); v=-v;} do{t[i++]='0'+v%10; v/=10;}while(v); while(i) bput(b,t[--i]); }
static void bhex(Buf*b,u64 v){ const char*h="0123456789abcdef"; bstr(b,"0x"); int st=0; for(int i=60;i>=0;i-=4){int d=(v>>i)&15; if(d||st||i==0){st=1;bput(b,h[d]);}} }
static void bflt(Buf*b,float f){ if(f<0){bput(b,'-');f=-f;} s64 ip=(s64)f; bint(b,ip); bput(b,'.'); s64 fr=(s64)((f-ip)*1000); if(fr<100)bput(b,'0'); if(fr<10)bput(b,'0'); bint(b,fr); }
static void bflush(Buf*b){ svc_log(b->b,b->n); b->n=0; }
__attribute__((visibility("hidden"))) void* memset(void*d,int c,unsigned long n){ volatile u8*p=d; while(n--) *p++=(u8)c; return d; }
__attribute__((visibility("hidden"))) void* memcpy(void*d,const void*s,unsigned long n){ volatile u8*p=d; const u8*q=s; while(n--) *p++=*q++; return d; }
