/* Game-owned Windows launch policy. Do not create a console on normal launch. */
#include "mmx_startup.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Shell pipes/files remain usable for scripts and benchmarks. Console handles
 * inherited by a GUI process are not necessarily attached or usable. */
static int redirected(FILE *stream, DWORD id) {
  DWORD type = GetFileType(GetStdHandle(id));
  return _fileno(stream) >= 0 && (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE);
}
static int open_log(const wchar_t *root, wchar_t *path, size_t count) {
  wchar_t folder[32768];
  if (swprintf(folder,32768,L"%ls\\logs",root)<0) return -1;
  if (!CreateDirectoryW(folder,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS) return -1;
  SYSTEMTIME now; GetLocalTime(&now);
  if (swprintf(path,count,L"%ls\\mmx-%04u%02u%02u-%02u%02u%02u-%lu.log",folder,
      now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,
      (unsigned long)GetCurrentProcessId())<0) return -1;
  return _wopen(path,_O_WRONLY|_O_CREAT|_O_EXCL|_O_BINARY,_S_IREAD|_S_IWRITE);
}
void MmxStartupLogging(void) {
  wchar_t root[32768],config[32768],path[32768];
  DWORD n=GetModuleFileNameW(NULL,root,32768);
  if (!n || n>=32768) return;
  wchar_t *slash=wcsrchr(root,L'\\'); if(!slash) return; *slash=0;
  if(swprintf(config,32768,L"%ls\\logging.ini",root)<0) return;
  int console=GetPrivateProfileIntW(L"Logging",L"Console",0,config)!=0;
  int out_redirected=redirected(stdout,STD_OUTPUT_HANDLE);
  int err_redirected=redirected(stderr,STD_ERROR_HANDLE);
  if(console && (GetConsoleWindow() || AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole())) {
    if(!out_redirected) freopen("CONOUT$","w",stdout);
    if(!err_redirected) freopen("CONOUT$","w",stderr);
    freopen("CONIN$","r",stdin);
  } else if(!out_redirected || !err_redirected) {
    int fd=open_log(root,path,32768);
    if(fd<0) {
      /* Read-only installations still retain diagnostics. */
      DWORD size=GetTempPathW(32768,root);
      if(size && size<32740) {
        wcscat(root,L"MegaManXSNESRecomp"); CreateDirectoryW(root,NULL);
        fd=open_log(root,path,32768);
      }
    }
    if(fd>=0) {
      /* GUI CRT streams may initially have descriptor -2. Opening NUL first
       * gives dup2 a real target descriptor without touching inherited pipes. */
      if(!out_redirected) { freopen("NUL","w",stdout); _dup2(fd,_fileno(stdout)); }
      if(!err_redirected) { freopen("NUL","w",stderr); _dup2(fd,_fileno(stderr)); }
      _close(fd);
    }
  }
  setvbuf(stdout,NULL,_IONBF,0); setvbuf(stderr,NULL,_IONBF,0);
  fprintf(stderr,"[mmx] session pid=%lu; console=%s\n",
      (unsigned long)GetCurrentProcessId(),console?"enabled":"off");
}

/* The GUI subsystem avoids even a brief console flash before main. Both the
 * MinGW and MSVC CRTs supply the already-parsed narrow argument vector. */
extern int main(int argc,char **argv);
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show) {
  (void)instance;(void)previous;(void)command;(void)show;
  return main(__argc,__argv);
}
#else
void MmxStartupLogging(void) { }
#endif
