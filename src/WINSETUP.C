/* Launch! 3.76 - Windows 3.x Program Manager registration helper.
   Microsoft C/C++ 7.0 + Windows 3.x SDK, Win16 medium model.
   Compatible target: Windows 3.0, 3.1 and 3.11/WfW. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#ifndef WM_DDE_INITIATE
#define WM_DDE_INITIATE  0x03E0
#endif
#ifndef WM_DDE_TERMINATE
#define WM_DDE_TERMINATE 0x03E1
#endif
#ifndef WM_DDE_ACK
#define WM_DDE_ACK       0x03E4
#endif
#ifndef WM_DDE_EXECUTE
#define WM_DDE_EXECUTE   0x03E8
#endif
#ifndef HWND_BROADCAST
#define HWND_BROADCAST ((HWND)0xFFFF)
#endif
#define DDE_FACK 0x8000

static HWND dde_server=(HWND)0;
static int dde_waiting_exec=0;
static int dde_exec_done=0;
static int dde_exec_positive=0;
static HGLOBAL dde_exec_handle=(HGLOBAL)0;
static HGLOBAL dde_ack_handle=(HGLOBAL)0;

/* Keep the Program Manager command and resolved PIF path in static storage.
   MSC 7 Win16 segmented builds must not depend on automatic-buffer lifetime
   while a DDE execute transaction is outstanding. */
static char setup_pif[144];
static char setup_dde[320];
static char setup_diag[512];

static LRESULT FAR PASCAL SetupWndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp)
{
  if(msg==WM_DDE_ACK){
    if(dde_waiting_exec && (HWND)wp==dde_server){
      dde_exec_positive=(LOWORD(lp)&DDE_FACK)?1:0;
      dde_ack_handle=(HGLOBAL)HIWORD(lp);
      dde_exec_done=1;
      return 0L;
    }
    if(dde_server==(HWND)0){dde_server=(HWND)wp;return 0L;}
  }
  if(msg==WM_DDE_TERMINATE){
    if((HWND)wp==dde_server)PostMessage(dde_server,WM_DDE_TERMINATE,(WPARAM)hwnd,0L);
    return 0L;
  }
  return DefWindowProc(hwnd,msg,wp,lp);
}

static int pump_until(DWORD start,DWORD timeout,int *flag)
{
  MSG msg;
  while(!*flag && GetTickCount()-start<timeout){
    while(PeekMessage(&msg,(HWND)0,0,0,PM_REMOVE)){
      TranslateMessage(&msg);DispatchMessage(&msg);
    }
  }
  return *flag;
}

static BOOL dde_exec_wait(HWND client,HWND server,const char *cmd)
{
  LPSTR p;DWORD start;int got_ack,positive;
  dde_exec_handle=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,(DWORD)strlen(cmd)+1L);
  if(!dde_exec_handle)return FALSE;
  p=(LPSTR)GlobalLock(dde_exec_handle);
  if(!p){GlobalFree(dde_exec_handle);dde_exec_handle=(HGLOBAL)0;return FALSE;}
  lstrcpy(p,cmd);GlobalUnlock(dde_exec_handle);
  dde_exec_done=0;dde_exec_positive=0;dde_ack_handle=(HGLOBAL)0;dde_waiting_exec=1;
  if(!PostMessage(server,WM_DDE_EXECUTE,(WPARAM)client,MAKELONG(0,dde_exec_handle))){
    dde_waiting_exec=0;GlobalFree(dde_exec_handle);dde_exec_handle=(HGLOBAL)0;return FALSE;
  }
  start=GetTickCount();got_ack=pump_until(start,5000L,&dde_exec_done);
  dde_waiting_exec=0;positive=dde_exec_positive;
  /* Program Manager returns the command-memory handle in the ACK.  The client
     owns that returned handle after the transaction, for both positive and
     negative acknowledgements. */
  if(got_ack && dde_ack_handle){GlobalFree(dde_ack_handle);dde_ack_handle=(HGLOBAL)0;}
  else if(!got_ack && dde_exec_handle)GlobalFree(dde_exec_handle);
  dde_exec_handle=(HGLOBAL)0;
  return (got_ack&&positive)?TRUE:FALSE;
}

static void trim_eol(char *s)
{
  char *p=strchr(s,'\r');if(p)*p=0;p=strchr(s,'\n');if(p)*p=0;
}

static void default_cfg_path(HINSTANCE hi,char *cfg,int size)
{
  char *p,*q;
  if(!GetModuleFileName(hi,cfg,size)){lstrcpyn(cfg,"C:\\LAUNCH\\WINSETUP.CFG",size);return;}
  p=strrchr(cfg,'\\');q=strrchr(cfg,'/');if(!p||(q&&q>p))p=q;
  if(p)lstrcpy(p+1,"WINSETUP.CFG");else lstrcpyn(cfg,"WINSETUP.CFG",size);
}

static int remove_self_from_run(const char *self)
{
  char oldrun[256],newrun[256],token[144];const char *p;int n,first=1;
  GetProfileString("windows","run","",oldrun,sizeof(oldrun));
  newrun[0]=0;p=oldrun;
  while(*p){
    while(*p==' '||*p=='\t')p++;
    if(!*p)break;
    n=0;
    while(*p&&*p!=' '&&*p!='\t'&&n<(int)sizeof(token)-1)token[n++]=*p++;
    token[n]=0;
    if(stricmp(token,self)){
      if(!first)lstrcat(newrun," ");
      if(strlen(newrun)+strlen(token)+1>=sizeof(newrun))return 0;
      lstrcat(newrun,token);first=0;
    }
  }
  if(!WriteProfileString("windows","run",newrun))return 0;
  WriteProfileString((LPCSTR)0,(LPCSTR)0,(LPCSTR)0);
  return 1;
}

int PASCAL WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show)
{
  HWND client;ATOM aApp,aTopic;WNDCLASS wc;DWORD start;int connected=0;
  char cfg[144],work[144],group[48],icon[144],winpath[144];
  char self[144],line[192],msg[320];FILE *f,*test;
  (void)hp;(void)show;

  default_cfg_path(hi,cfg,sizeof(cfg));
  if(cmd&&*cmd){
    while(*cmd==' '||*cmd=='\t')cmd++;
    if(*cmd=='\"'){
      char *e;cmd++;lstrcpyn(cfg,cmd,sizeof(cfg));e=strchr(cfg,'\"');if(e)*e=0;
    }else lstrcpyn(cfg,cmd,sizeof(cfg));
  }
  lstrcpy(setup_pif,"C:\\LAUNCH\\LAUNCH.PIF");lstrcpy(work,"C:\\LAUNCH");lstrcpy(group,"Launch!");
  lstrcpy(icon,"C:\\LAUNCH\\LAUNCH.ICO");lstrcpy(winpath,"C:\\WINDOWS");
  f=fopen(cfg,"rt");
  if(!f){wsprintf(msg,"Launch! Windows Setup could not open:\n%s",cfg);MessageBox((HWND)0,msg,"Launch! Windows Setup",MB_OK|MB_ICONSTOP);return 1;}
  while(fgets(line,sizeof(line),f)){
    trim_eol(line);
    if(!strnicmp(line,"PIF=",4))lstrcpyn(setup_pif,line+4,sizeof(setup_pif));
    else if(!strnicmp(line,"WorkDir=",8))lstrcpyn(work,line+8,sizeof(work));
    else if(!strnicmp(line,"Group=",6))lstrcpyn(group,line+6,sizeof(group));
    else if(!strnicmp(line,"Icon=",5))lstrcpyn(icon,line+5,sizeof(icon));
    else if(!strnicmp(line,"WinPath=",8))lstrcpyn(winpath,line+8,sizeof(winpath));
  }
  fclose(f);
  test=fopen(setup_pif,"rb");
  if(!test){wsprintf(msg,"Launch! Windows Setup could not find:\n%s",setup_pif);MessageBox((HWND)0,msg,"Launch! Windows Setup",MB_OK|MB_ICONSTOP);return 2;}fclose(test);

  memset(&wc,0,sizeof(wc));wc.lpfnWndProc=SetupWndProc;wc.hInstance=hi;wc.lpszClassName="LaunchWinSetup";
  if(!RegisterClass(&wc)){WNDCLASS oldwc;if(!GetClassInfo(hi,"LaunchWinSetup",&oldwc)){MessageBox((HWND)0,"Could not register the setup window class.","Launch! Windows Setup",MB_OK|MB_ICONSTOP);return 3;}}
  client=CreateWindow("LaunchWinSetup","Launch! Setup",WS_OVERLAPPED,0,0,0,0,(HWND)0,(HMENU)0,hi,(LPSTR)0);
  if(!client){MessageBox((HWND)0,"Could not create the setup window.","Launch! Windows Setup",MB_OK|MB_ICONSTOP);return 4;}

  aApp=GlobalAddAtom("PROGMAN");aTopic=GlobalAddAtom("PROGMAN");dde_server=(HWND)0;
  SendMessage(HWND_BROADCAST,WM_DDE_INITIATE,(WPARAM)client,MAKELONG(aApp,aTopic));
  start=GetTickCount();while(dde_server==(HWND)0&&GetTickCount()-start<4000L){MSG m;while(PeekMessage(&m,(HWND)0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}}
  connected=dde_server!=(HWND)0;
  if(!connected){MessageBox((HWND)0,"Program Manager did not respond. Make sure Program Manager is running.","Launch! Windows Setup",MB_OK|MB_ICONSTOP);goto done;}

  wsprintf(setup_dde,"[CreateGroup(%s)]",group);
  if(!dde_exec_wait(client,dde_server,setup_dde)){MessageBox((HWND)0,"Program Manager rejected CreateGroup.","Launch! Windows Setup",MB_OK|MB_ICONSTOP);connected=0;goto done;}

  /* Construct AddItem once in persistent storage.  Show exactly what will be
     sent during this test build so DDE syntax and path resolution can be
     verified independently of the acknowledgement path. */
  wsprintf(setup_dde,"[AddItem(%s,Launch!)]",setup_pif);
  wsprintf(setup_diag,"Sending to Program Manager:\n\n%s\n\nPIF:\n%s",setup_dde,setup_pif);
  MessageBox((HWND)0,setup_diag,"Launch! Windows Setup - AddItem Test",MB_OK|MB_ICONINFORMATION);

  if(!dde_exec_wait(client,dde_server,setup_dde)){
    wsprintf(setup_diag,"Program Manager rejected AddItem.\n\nCommand:\n%s\n\nPIF:\n%s",setup_dde,setup_pif);
    MessageBox((HWND)0,setup_diag,"Launch! Windows Setup",MB_OK|MB_ICONSTOP);connected=0;goto done;
  }

  if(!GetModuleFileName(hi,self,sizeof(self)))lstrcpy(self,work),lstrcat(self,"\\WINSETUP.EXE");
  if(!remove_self_from_run(self)){
    MessageBox((HWND)0,"The Launch! group was created, but WINSETUP could not remove itself from WIN.INI.\nIt may run again next time Windows starts.","Launch! Windows Setup",MB_OK|MB_ICONEXCLAMATION);
  }else{
    MessageBox((HWND)0,"Launch! Program Manager group created.","Launch! Windows Setup",MB_OK|MB_ICONINFORMATION);
  }

done:
  if(dde_server!=(HWND)0)PostMessage(dde_server,WM_DDE_TERMINATE,(WPARAM)client,0L);
  if(aApp)GlobalDeleteAtom(aApp);if(aTopic)GlobalDeleteAtom(aTopic);DestroyWindow(client);
  return connected?0:5;
}
