// ---- SD card files (nn::fs). The settings file needs them in the release build too; the debug build also uses them for its
// command channel. main.npdm must grant the SdCard filesystem permission (tools/mknpdm.py). ----
typedef struct { void* h; } FH;
typedef struct { int flags; } WOpt;
IMPORT(u32,_ZN2nn2fs11MountSdCardEPKc,FsMountSdCard,const char*)
IMPORT(u32,_ZN2nn2fs19MountSdCardForDebugEPKc,FsMountSd,const char*)
IMPORT(u32,_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci,FsOpen,FH*,const char*,int)
IMPORT(u32,_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE,FsSize,s64*,FH)
IMPORT(u32,_ZN2nn2fs8ReadFileEPmNS0_10FileHandleElPvmRKNS0_10ReadOptionE,FsReadN,u64*,FH,s64,void*,u64,const WOpt*)   // the overload with the byte count: a short read (the host rewriting cmd.txt) is not an abort
IMPORT(u32,_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE,FsWrite,FH,s64,const void*,u64,const WOpt*)
IMPORT(void,_ZN2nn2fs9CloseFileENS0_10FileHandleE,FsClose,FH)
IMPORT(u32,_ZN2nn2fs10CreateFileEPKcl,FsCreate,const char*,s64)
IMPORT(u32,_ZN2nn2fs10DeleteFileEPKc,FsDelete,const char*)
IMPORT(u32,_ZN2nn2fs15CreateDirectoryEPKc,FsMkdir,const char*)
#define FsMountSdCard FsMountSdCard_p()
#define FsMountSd FsMountSd_p()
#define FsOpen FsOpen_p()
#define FsSize FsSize_p()
#define FsReadN FsReadN_p()
#define FsWrite FsWrite_p()
#define FsClose FsClose_p()
#define FsCreate FsCreate_p()
#define FsDelete FsDelete_p()
#define FsMkdir FsMkdir_p()
// mount the SD card as "sd:" once (the retail call; the debug one as a fallback); 0 = mounted
static u32 g_sd_rc=1; static int g_sd_tried;
static u32 sd_mount(void){ if(g_sd_tried) return g_sd_rc; g_sd_tried=1;
  g_sd_rc=FsMountSdCard("sd"); if(g_sd_rc) g_sd_rc=FsMountSd("sd"); return g_sd_rc; }
