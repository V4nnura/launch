/*
 * Launch! Menu Manager helper for Windows 3.0/3.1.
 *
 * Program Manager launches this tiny task for the dedicated Menu Manager
 * icon.  It deliberately does not depend on Program Manager preserving a
 * command-line switch for !WIN16.EXE.
 */
#ifndef WINVER
#define WINVER 0x0300
#endif
#include <windows.h>
#include <string.h>

#define WM_SHOW_MENU_MANAGER (WM_USER+20)

int PASCAL WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show)
{
  HWND prior;
  char path[160];
  char *p;
  UINT rc;
  (void)prev;(void)cmd;(void)show;

  prior=FindWindow("LaunchWin16Button",NULL);
  if(prior){
    BringWindowToTop(prior);
    SetActiveWindow(prior);
    SendMessage(prior,WM_SHOW_MENU_MANAGER,0,0L);
    return 0;
  }

  path[0]=0;
  if(!GetModuleFileName(inst,path,sizeof(path))){
    MessageBox(NULL,"Windows could not determine the Menu Manager path.","Launch!",MB_OK|MB_ICONSTOP);
    return 1;
  }
  path[sizeof(path)-1]=0;
  p=strrchr(path,'\\');
  if(p)strcpy(p+1,"!WIN16.EXE /MANAGE");
  else strcpy(path,"!WIN16.EXE /MANAGE");

  rc=(UINT)WinExec(path,SW_SHOWNORMAL);
  if(rc<32){
    MessageBox(NULL,"Could not start Launch! Menu Manager.","Launch!",MB_OK|MB_ICONSTOP);
    return 1;
  }
  return 0;
}
