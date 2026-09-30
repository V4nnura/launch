/* Launch! 3.75 - native Windows 3.x menu companion (initial POC).
   Target: Microsoft C/C++ 7.0 + Windows 3.0/3.1 SDK, medium model. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <commdlg.h>

#define MAX_NODES 128
#define MAX_SECTIONS 32
#define MAX_LINE 256
#define IDM_FIRST 100
#define IDM_RUN 899
#define IDM_EXPLORE 900
#define IDM_EXITWIN 901
#define RUN_EDIT 20
#define RUN_OK 21
#define RUN_CANCEL 22
#define RUN_BROWSE 23
#define RUN_MIN 24
#define BTN_ID 1
#define RAISE_TIMER 1

typedef struct {
  char title[48];
  char command[160];
  int section_index;
  int child_section_index;
  int is_folder;
  int enter_after;
  int change_dir;
  int prompt;
  int add_path;
} MENU_NODE;

typedef struct { char name[128]; HMENU menu; } SECTION;

static HINSTANCE gInst;
static HWND gWnd, gButton, runWnd, runEdit, runMin;
static HBRUSH gWindowBrush;
static HFONT gDialogFont;
static MENU_NODE nodes[MAX_NODES];
static int node_count;
static SECTION sections[MAX_SECTIONS];
static int section_count;
static char base_dir[144];
static char menu_path[160];
static char winrun_path[160];
static HMENU root_menu;
static int menu_tracking;

static void trim(char *s)
{
  char *p=s; int n;
  while(*p==' '||*p=='\t')p++;
  if(p!=s)memmove(s,p,strlen(p)+1);
  n=strlen(s);while(n>0&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;
}

static void get_base_dir(void)
{
  char exe[160],*q;
  GetModuleFileName(gInst,exe,sizeof(exe));
  q=strrchr(exe,'\\');
  if(q)*q=0; else strcpy(exe,".");
  strcpy(base_dir,exe);
  sprintf(menu_path,"%s\\LAUNCH.MNU",base_dir);
  sprintf(winrun_path,"%s\\WINRUN.BAT",base_dir);
}

static int find_section(const char *name)
{
  int i;for(i=0;i<section_count;i++)if(!stricmp(sections[i].name,name))return i;return -1;
}
static int ensure_section(const char *name)
{
  int i=find_section(name);if(i>=0)return i;
  if(section_count>=MAX_SECTIONS)return -1;
  strncpy(sections[section_count].name,name,sizeof(sections[0].name)-1);
  sections[section_count].name[sizeof(sections[0].name)-1]=0;
  sections[section_count].menu=NULL;
  return section_count++;
}

static int split_item(char *s,char *f[],int maxf)
{
  int n=0;char *p=s; if(maxf<1)return 0;f[n++]=p;
  while(*p&&n<maxf){if(*p=='|'){*p=0;f[n++]=p+1;}p++;}return n;
}

static int load_launch_menu(void)
{
  FILE *f;char line[MAX_LINE],section[128]="Launcher";int section_index;
  node_count=0;section_count=0;
  f=fopen(menu_path,"rt");
  if(!f)return 0;
  section_index=ensure_section("Launcher");
  while(fgets(line,sizeof(line),f)){
    char *fields[6];int nf;MENU_NODE *n;
    trim(line);if(!line[0]||line[0]==';')continue;
    if(line[0]=='['){char *r=strchr(line,']');if(r){*r=0;strncpy(section,line+1,sizeof(section)-1);section[sizeof(section)-1]=0;section_index=ensure_section(section);}continue;}
    if(node_count>=MAX_NODES)continue;
    if(!strnicmp(line,"FOLDER=",7)){
      n=&nodes[node_count++];memset(n,0,sizeof(*n));n->is_folder=1;
      strncpy(n->title,line+7,sizeof(n->title)-1);trim(n->title);
      n->section_index=section_index;
      { char child[128];sprintf(child,"%s\\%s",section,n->title);n->child_section_index=ensure_section(child); }
    } else if(!strnicmp(line,"ITEM=",5)){
      n=&nodes[node_count++];memset(n,0,sizeof(*n));
      nf=split_item(line+5,fields,6);if(nf<2){node_count--;continue;}
      strncpy(n->title,fields[0],sizeof(n->title)-1);strncpy(n->command,fields[1],sizeof(n->command)-1);
      n->section_index=section_index;
      if(nf>2)n->enter_after=atoi(fields[2]);if(nf>3)n->change_dir=atoi(fields[3]);
      if(nf>4)n->prompt=atoi(fields[4]);if(nf>5)n->add_path=atoi(fields[5]);
    } else if(!strnicmp(line,"SEPARATOR=",10)||!stricmp(line,"SEPARATOR")){
      n=&nodes[node_count++];memset(n,0,sizeof(*n));strcpy(n->title,"-");n->section_index=section_index;
    }
  }
  fclose(f);return 1;
}

static HMENU menu_for_section(const char *name){int i=find_section(name);return i>=0?sections[i].menu:NULL;}
static int is_windows_exe(const char *cmd);

static void build_native_menu(void)
{
  int i;HMENU m,sub;
  /* Create a fresh native USER menu tree for each invocation.  The earlier
     implementation leaked popup-menu handles every time the Launch button
     was used, which is especially harmful to the small Win3.x USER heap. */
  for(i=0;i<section_count;i++)sections[i].menu=CreatePopupMenu();
  for(i=0;i<node_count;i++){
    m=(nodes[i].section_index>=0&&nodes[i].section_index<section_count)?sections[nodes[i].section_index].menu:NULL;if(!m)continue;
    if(!strcmp(nodes[i].title,"-")){AppendMenu(m,MF_SEPARATOR,0,NULL);continue;}
    if(nodes[i].is_folder){sub=(nodes[i].child_section_index>=0&&nodes[i].child_section_index<section_count)?sections[nodes[i].child_section_index].menu:NULL;if(sub)AppendMenu(m,MF_POPUP,(UINT)sub,nodes[i].title);}
    else {
      char label[52];
      strncpy(label,nodes[i].title,sizeof(label)-1);label[sizeof(label)-1]=0;
      AppendMenu(m,MF_STRING,IDM_FIRST+i,label);
    }
  }
  root_menu=menu_for_section("Launcher");
  if(root_menu){AppendMenu(root_menu,MF_SEPARATOR,0,NULL);AppendMenu(root_menu,MF_STRING,IDM_RUN,"Run...");AppendMenu(root_menu,MF_STRING,IDM_EXPLORE,"Explore");AppendMenu(root_menu,MF_STRING,IDM_EXITWIN,"Exit Windows");}
}

static void first_token(const char *cmd,char *out,int max)
{
  int i=0;const char *p=cmd;while(*p==' '||*p=='\t')p++;
  if(*p=='\"'){p++;while(*p&&*p!='\"'&&i<max-1)out[i++]=*p++;}
  else while(*p&&*p!=' '&&*p!='\t'&&i<max-1)out[i++]=*p++;
  out[i]=0;
}

static int is_windows_exe(const char *cmd)
{
  char token[144],path[160];FILE *f;unsigned char sig[2];long neoff;unsigned char b[4];
  first_token(cmd,token,sizeof(token));if(!token[0])return 0;
  strcpy(path,token);
  f=fopen(path,"rb");
  if(!f && !strchr(path,'\\') && !strchr(path,':')){sprintf(path,"%s\\%s",base_dir,token);f=fopen(path,"rb");}
  if(!f){char wdir[144];GetWindowsDirectory(wdir,sizeof(wdir));sprintf(path,"%s\\%s",wdir,token);f=fopen(path,"rb");}
  if(!f)return 0;
  if(fread(sig,1,2,f)!=2||sig[0]!='M'||sig[1]!='Z'){fclose(f);return 0;}
  if(fseek(f,0x3c,SEEK_SET)||fread(b,1,4,f)!=4){fclose(f);return 0;}
  neoff=(long)b[0]|((long)b[1]<<8)|((long)b[2]<<16)|((long)b[3]<<24);
  if(fseek(f,neoff,SEEK_SET)||fread(sig,1,2,f)!=2){fclose(f);return 0;}fclose(f);
  return sig[0]=='N'&&sig[1]=='E';
}

static void run_dos(const char *cmd)
{
  FILE *f;char shell[220];
  f=fopen(winrun_path,"wt");if(!f){MessageBox(gWnd,"Could not create WINRUN.BAT.","Launch!",MB_OK|MB_ICONSTOP);return;}
  fprintf(f,"@ECHO OFF\n%s\nPAUSE\n",cmd);fclose(f);
  sprintf(shell,"COMMAND.COM /C %s",winrun_path);
  if(WinExec(shell,SW_SHOWNORMAL)<32)MessageBox(gWnd,"Windows could not start the DOS command.","Launch!",MB_OK|MB_ICONSTOP);
}

static void run_item(int idx)
{
  if(idx<0||idx>=node_count||nodes[idx].is_folder)return;
  if(is_windows_exe(nodes[idx].command)){
    if(WinExec(nodes[idx].command,SW_SHOWNORMAL)<32)MessageBox(gWnd,"Windows could not start this program.","Launch!",MB_OK|MB_ICONSTOP);
  } else run_dos(nodes[idx].command);
}

static void show_launch_menu(void)
{
  POINT pt;int i;
  if(!load_launch_menu()){MessageBox(gWnd,"LAUNCH.MNU could not be opened.","Launch!",MB_OK|MB_ICONSTOP);return;}
  build_native_menu();if(!root_menu)return;
  /* Let USER own the entire popup/cascade while it is active.  In particular
     do not run our Win3.x pseudo-topmost repaint timer underneath a popup
     menu: display drivers that use save-under (notably some 256-colour
     drivers) can otherwise restore stale menu-border pixels. */
  menu_tracking=1;
  pt.x=0;pt.y=0;ClientToScreen(gWnd,&pt);
  { RECT r;GetClientRect(gWnd,&r);pt.y+=r.bottom; }
  TrackPopupMenu(root_menu,TPM_LEFTALIGN|TPM_LEFTBUTTON,pt.x,pt.y,0,gWnd,NULL);
  menu_tracking=0;
  DestroyMenu(root_menu);root_menu=NULL;
  for(i=0;i<section_count;i++)sections[i].menu=NULL;
  SetWindowPos(gWnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  InvalidateRect(gWnd,NULL,FALSE);UpdateWindow(gWnd);
}

static void button_size(int *bw,int *bh)
{
  int cw=GetSystemMetrics(SM_CXSIZE),ch=GetSystemMetrics(SM_CYSIZE);
  *bw=cw+8;
  *bh=ch>18?ch:18;
}

static void raise_button(HWND h)
{
  /* Z-order only: never change the canonical (24,0) geometry here. */
  SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}

static void execute_run_dialog(void)
{
  char cmd[256];int show=IsDlgButtonChecked(runWnd,RUN_MIN)?SW_SHOWMINIMIZED:SW_SHOWNORMAL;
  GetWindowText(runEdit,cmd,sizeof(cmd));
  if(cmd[0]){if(WinExec(cmd,show)<32)MessageBox(runWnd,"Windows could not run this command.","Run",MB_OK|MB_ICONSTOP);else DestroyWindow(runWnd);}
}

static int browse_for_program(HWND owner,char *file,int maxfile)
{
  HINSTANCE lib;FARPROC proc;OPENFILENAME of;BOOL ok;
  lib=LoadLibrary("COMMDLG.DLL");
  if((UINT)lib<32){
    MessageBox(owner,"Browse requires the Windows common-dialog library.","Run",MB_OK|MB_ICONINFORMATION);
    return 0;
  }
  proc=GetProcAddress(lib,"GetOpenFileName");
  if(!proc){FreeLibrary(lib);return 0;}
  memset(&of,0,sizeof(of));of.lStructSize=sizeof(of);of.hwndOwner=owner;
  of.lpstrFilter="Programs (*.EXE;*.COM;*.BAT)\0*.EXE;*.COM;*.BAT\0All Files (*.*)\0*.*\0\0";
  of.lpstrFile=file;of.nMaxFile=maxfile;of.Flags=OFN_FILEMUSTEXIST|OFN_HIDEREADONLY;
  ok=((BOOL (FAR PASCAL *)(LPOPENFILENAME))proc)(&of);
  FreeLibrary(lib);return ok?1:0;
}

static LRESULT FAR PASCAL RunProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  switch(msg){
    case WM_CREATE:{
      HWND label,ok,cancel,browse;
      label=CreateWindow("STATIC","Command Line:",WS_CHILD|WS_VISIBLE,8,10,90,14,h,NULL,gInst,NULL);
      runEdit=CreateWindow("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,8,27,235,20,h,(HMENU)RUN_EDIT,gInst,NULL);
      runMin=CreateWindow("BUTTON","Run Minimized",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,8,58,120,18,h,(HMENU)RUN_MIN,gInst,NULL);
      ok=CreateWindow("BUTTON","OK",WS_CHILD|WS_VISIBLE|BS_DEFPUSHBUTTON,252,8,72,22,h,(HMENU)RUN_OK,gInst,NULL);
      cancel=CreateWindow("BUTTON","Cancel",WS_CHILD|WS_VISIBLE,252,37,72,22,h,(HMENU)RUN_CANCEL,gInst,NULL);
      browse=CreateWindow("BUTTON","Browse...",WS_CHILD|WS_VISIBLE,252,66,72,22,h,(HMENU)RUN_BROWSE,gInst,NULL);
      if(gDialogFont){SendMessage(label,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(runEdit,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(runMin,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(ok,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(cancel,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(browse,WM_SETFONT,(WPARAM)gDialogFont,0);}
      SetFocus(runEdit);return 0;}
    case WM_COMMAND:
      if(wp==RUN_OK){execute_run_dialog();return 0;}
      if(wp==RUN_CANCEL){DestroyWindow(h);return 0;}
      if(wp==RUN_BROWSE){char file[160]="";if(browse_for_program(h,file,sizeof(file)))SetWindowText(runEdit,file);return 0;}
      break;
    case WM_CTLCOLOR:
      if((HWND)LOWORD(lp)!=runEdit){SetBkColor((HDC)wp,GetSysColor(COLOR_WINDOW));SetBkMode((HDC)wp,TRANSPARENT);return (LRESULT)gWindowBrush;}
      break;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:runWnd=NULL;return 0;
  }
  return DefWindowProc(h,msg,wp,lp);
}

static void show_run_dialog(void)
{
  if(runWnd){SetActiveWindow(runWnd);return;}
  runWnd=CreateWindow("LaunchRunDialog","Run",WS_POPUP|WS_CAPTION|WS_SYSMENU,120,100,338,128,gWnd,NULL,gInst,NULL);
  if(runWnd){ShowWindow(runWnd,SW_SHOW);UpdateWindow(runWnd);}
}

static void paint_launch_button(HWND h,HDC dc,int pressed)
{
  RECT r;HBRUSH face;HPEN hi,sh,oldp;
  GetClientRect(h,&r);
  face=CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
  FillRect(dc,&r,face);DeleteObject(face);
  hi=CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNHIGHLIGHT));
  sh=CreatePen(PS_SOLID,1,GetSysColor(COLOR_BTNSHADOW));
  oldp=(HPEN)SelectObject(dc,pressed?sh:hi);
  MoveTo(dc,r.left,r.bottom-1);LineTo(dc,r.left,r.top);LineTo(dc,r.right-1,r.top);
  SelectObject(dc,pressed?hi:sh);
  MoveTo(dc,r.right-1,r.top);LineTo(dc,r.right-1,r.bottom-1);LineTo(dc,r.left,r.bottom-1);
  SelectObject(dc,oldp);DeleteObject(hi);DeleteObject(sh);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,0,0));
  if(gDialogFont)SelectObject(dc,gDialogFont);
  if(pressed)OffsetRect(&r,1,1);
  DrawText(dc,"!",1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}

static LRESULT FAR PASCAL WndProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  static int pressed=0;
  switch(msg){
    case WM_CREATE:SetTimer(h,RAISE_TIMER,100,NULL);return 0;
    case WM_ACTIVATEAPP:
      if(!wp && !menu_tracking){
        /* Another application just became active.  Keep the launcher above
           its titlebar without taking activation back from it. */
        SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        InvalidateRect(h,NULL,FALSE);UpdateWindow(h);
      }
      return 0;
    case WM_TIMER:
      if(wp==RAISE_TIMER && !menu_tracking && GetTopWindow(NULL)!=h){
        /* Windows 3.x has no WS_EX_TOPMOST.  Only when another top-level
           window has overtaken us, restore our Z-order without activation.
           Repaint once at that point so pixels from the covering window can
           never remain in the custom button surface. */
        SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        InvalidateRect(h,NULL,FALSE);
        UpdateWindow(h);
      }
      return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paint_launch_button(h,dc,pressed);EndPaint(h,&ps);return 0;}
    case WM_LBUTTONDOWN:pressed=1;SetCapture(h);InvalidateRect(h,NULL,FALSE);return 0;
    case WM_LBUTTONUP:
      if(pressed){POINT pt;RECT r;pressed=0;ReleaseCapture();InvalidateRect(h,NULL,FALSE);pt.x=LOWORD(lp);pt.y=HIWORD(lp);GetClientRect(h,&r);if(PtInRect(&r,pt))show_launch_menu();}
      return 0;
    case WM_COMMAND:
      if(wp==IDM_RUN){show_run_dialog();return 0;}
      if(wp==IDM_EXPLORE){WinExec("WINFILE.EXE",SW_SHOWNORMAL);return 0;}
      if(wp==IDM_EXITWIN){ExitWindows(0,0);return 0;}
      if(wp>=IDM_FIRST&&wp<IDM_FIRST+MAX_NODES){run_item((int)wp-IDM_FIRST);return 0;}
      break;
    case WM_DESTROY:KillTimer(h,RAISE_TIMER);PostQuitMessage(0);return 0;
  }
  return DefWindowProc(h,msg,wp,lp);
}

int PASCAL WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show)
{
  WNDCLASS wc;MSG msg;HDC dc;(void)cmd;(void)show;gInst=inst;gWindowBrush=CreateSolidBrush(GetSysColor(COLOR_WINDOW));get_base_dir();
  dc=GetDC(NULL);gDialogFont=CreateFont(-MulDiv(8,GetDeviceCaps(dc,LOGPIXELSY),72),0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");ReleaseDC(NULL,dc);
  if(!prev){memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=NULL;wc.lpszClassName="LaunchWin16Button";
    if(!RegisterClass(&wc)){MessageBox(NULL,"Could not register Launch! Win16 window class.","Launch!",MB_OK|MB_ICONSTOP);return 1;}
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=RunProc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName="LaunchRunDialog";
    if(!RegisterClass(&wc)){MessageBox(NULL,"Could not register Run dialog class.","Launch!",MB_OK|MB_ICONSTOP);return 1;}}
  /* The popup itself is the button.  This removes the four exposed host
     corner pixels produced by a rounded stock child BUTTON.  Keep the proven
     24-pixel physical left offset and paint a conventional Win3.x 3-D face. */
  { int bw,bh; button_size(&bw,&bh);
    gWnd=CreateWindow("LaunchWin16Button","",WS_POPUP,24,0,bw,bh,NULL,NULL,inst,NULL);
  }
  if(!gWnd){MessageBox(NULL,"Could not create Launch! Win16 button.","Launch!",MB_OK|MB_ICONSTOP);return 1;}
  ShowWindow(gWnd,SW_SHOWNOACTIVATE);
  SetWindowPos(gWnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  UpdateWindow(gWnd);
  while(GetMessage(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}if(gDialogFont)DeleteObject(gDialogFont);if(gWindowBrush)DeleteObject(gWindowBrush);return (int)msg.wParam;
}
