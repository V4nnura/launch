/* Launch! 3.76 - native Windows 3.x menu companion.
   Target: Microsoft C/C++ 7.0 + Windows 3.0/3.1 SDK, medium model. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <commdlg.h>
#include <dde.h>

#define MAX_NODES 128
#define MAX_SECTIONS 32  /* Keep Win16 medium-model DGROUP within the 64K limit. */
#define MAX_LINE 256
#define IDM_FIRST 100
#define IDM_RUN 899
#define IDM_EXPLORE 900
#define IDM_EXITWIN 901
#define IDD_RUN 1000
#define IDD_LAUNCHER 1001
#define IDD_FOLDER 1002
#define IDD_CONFIRM 1003
#define IDD_EXITWIN 1004
#define IDD_MESSAGE 1005
#define CONFIRM_TEXT 70
#define CONFIRM_ICON 71
#define MESSAGE_TEXT 72
#define MESSAGE_ICON 73
#define IDI_SLEEP 1100
#define IDI_CONFIRM 1101
#define IDI_ERROR 1102
#define IDI_SUCCESS 1103
#define RUN_EDIT 20
#define RUN_OK IDOK
#define RUN_CANCEL IDCANCEL
#define RUN_BROWSE 23
#define RUN_MIN 24
#define BTN_ID 1
#define RAISE_TIMER 1
#define WM_SHOW_LAUNCH_MENU (WM_USER+16)
#define WM_MANAGE_ACTION (WM_USER+17)
#define WM_MANAGE_SELECTED (WM_USER+18)
#define WM_BEGIN_BUTTON_DRAG (WM_USER+19)
#ifndef TPM_BOTTOMALIGN
#define TPM_BOTTOMALIGN 0x0020
#endif
#ifndef VK_OEM_5
#define VK_OEM_5 0xDC
#endif
#ifndef SC_HOTKEY
#define SC_HOTKEY 0xF150
#endif
#ifndef STM_SETICON
#define STM_SETICON 0x0170
#endif

#define IDM_MGMT_ADD_LAUNCHER 920
#define IDM_MGMT_ADD_FOLDER 921
#define IDM_MGMT_ADD_SEPARATOR 922
#define IDM_MGMT_EDIT 923
#define IDM_MGMT_REMOVE 924
#define IDM_MGMT_REORDER 925
#define IDM_EDIT_FIRST 1200
#define IDM_REMOVE_FIRST 1400

#define MANAGE_NONE 0
#define MANAGE_EDIT 1
#define MANAGE_REMOVE 2
#define MANAGE_MENU 3

#define MSG_ERROR 1
#define MSG_WARNING 2
#define MSG_INFO 3
#define MSG_SUCCESS 4

#define ITEM_NAME 40
#define ITEM_COMMAND 41
#define ITEM_PARAMS 42
#define ITEM_PROMPT 43
#define ITEM_ENTER 44
#define ITEM_CHDIR 45
#define ITEM_ADDPATH 46
#define ITEM_OK IDOK
#define ITEM_CANCEL IDCANCEL
#define ITEM_BROWSE 49

#define REORDER_LIST 60
#define REORDER_UP 61
#define REORDER_DOWN 62
#define REORDER_CLOSE 63

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
static HFONT gDialogFont,gButtonFont;
static MENU_NODE nodes[MAX_NODES];
static int node_count;
static SECTION sections[MAX_SECTIONS];
static int section_count;
static char base_dir[144];
static char menu_path[160];
static int menu_path_explicit;
static char winrun_path[160];
static char launch_cfg_path[160];
static char suite_title[32]="Launch!";
static int menu_open_pending=0;
static int menu_open_delay=0;
static char win16_ini_path[160];
static HMENU root_menu;
static int menu_tracking;
static int win_accent_index=12;
static int win_launcher_index=10;
static int button_x=24;
static int button_edge=0; /* 0=top, 1=bottom */
static int manage_mode=MANAGE_NONE;
static HWND itemWnd,itemName,itemCommand,itemParams,itemPrompt,itemEnter,itemChdir,itemAddpath;
static int item_dialog_done,item_dialog_ok,item_dialog_folder,item_dialog_editing,item_dialog_node;
static int item_dialog_section;
static MENU_NODE item_work;
static char item_exe[160],item_params[160];
static char confirm_title[48],confirm_text[180];
static int confirm_dialog_id=IDD_CONFIRM;
static char message_title[48],message_text[220];
static int message_kind;
static HWND reorderWnd,reorderList;
static int reorder_done;
static int reorder_map[MAX_NODES];
static int reorder_rows;
static HWND dde_server;
static int dde_initiating;
static int dde_waiting;
static int dde_request_ok;
static HGLOBAL dde_reply;
static FARPROC gButtonProc;
static void resize_button_for_mode(void);
static int browse_for_program(HWND owner,char *file,int maxfile);
static int show_confirm_dialog(const char *title,const char *text);
static int show_exit_windows_dialog(void);
static void show_message_dialog(HWND owner,const char *title,const char *text,int kind);
static int ensure_menu_file(void);
static int copy_file_local(const char *src,const char *dst);
static LRESULT FAR PASCAL ButtonProc(HWND h,UINT msg,WPARAM wp,LPARAM lp);

static void trim(char *s)
{
  char *p=s; int n;
  while(*p==' '||*p=='\t')p++;
  if(p!=s)memmove(s,p,strlen(p)+1);
  n=strlen(s);while(n>0&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;
}

static int get_base_dir(void)
{
  char exe[160],*q;int n;
  exe[0]=0;n=GetModuleFileName(gInst,exe,sizeof(exe));
  if(!n || n>=(int)sizeof(exe)){
    MessageBox(NULL,"Windows could not determine the !WIN16.EXE path.","Launch!",MB_OK|MB_ICONSTOP);
    return 0;
  }
  exe[sizeof(exe)-1]=0;
  q=strrchr(exe,'\\');
  if(!q){
    MessageBox(NULL,"Windows returned !WIN16.EXE without a directory path.","Launch!",MB_OK|MB_ICONSTOP);
    return 0;
  }
  *q=0;
  strcpy(base_dir,exe);
  sprintf(menu_path,"%s\\LAUNCH.MNU",base_dir);
  sprintf(winrun_path,"%s\\WINRUN.BAT",base_dir);
  sprintf(launch_cfg_path,"%s\\LAUNCH.CFG",base_dir);
  sprintf(win16_ini_path,"%s\\LAUNCH16.INI",base_dir);
  return 1;
}

static int is_menu_filename(const char *s)
{
  const char *dot=strrchr(s,'.');
  return dot && !stricmp(dot,".MNU");
}

static void set_menu_path_from_command_line(LPSTR cmd)
{
  char arg[160];char *p=cmd;int n=0;
  menu_path_explicit=0;
  while(*p==' '||*p=='\t')p++;
  if(!strnicmp(p,"/USE=",5)||!strnicmp(p,"-USE=",5))p+=5;
  while(*p==' '||*p=='\t')p++;
  if(!*p)return;
  /* Win16 is an 8.3-only target.  Do not add or interpret quoted paths.
     Only an actual .MNU filename is a menu override.  Some Win3.x launch
     paths can leave a stray character in WinMain's raw command tail; that
     must never replace the default adjacent LAUNCH.MNU. */
  while(*p&&*p!=' '&&*p!='\t'&&n<(int)sizeof(arg)-1)arg[n++]=*p++;
  arg[n]=0;trim(arg);
  if(!arg[0]||!is_menu_filename(arg))return;
  menu_path_explicit=1;
  if(strchr(arg,':')||arg[0]=='\\')strncpy(menu_path,arg,sizeof(menu_path)-1);
  else sprintf(menu_path,"%s\\%s",base_dir,arg);
  menu_path[sizeof(menu_path)-1]=0;
}

static void menu_sidecar_path(char *out,const char *ext)
{
  char *slash,*dot;
  strncpy(out,menu_path,179);out[179]=0;
  slash=strrchr(out,'\\');dot=strrchr(out,'.');
  if(dot && (!slash||dot>slash))*dot=0;
  strncat(out,ext,179-strlen(out));
}

static void load_win_preferences(void)
{
  FILE *f;char line[96],*eq;int v;
  win_accent_index=12;win_launcher_index=10;strcpy(suite_title,"Launch!");
  f=fopen(launch_cfg_path,"rt");
  if(f){
    while(fgets(line,sizeof(line),f)){
      trim(line);
      if(!line[0]||line[0]==';'||line[0]=='#')continue;
      eq=strchr(line,'=');if(!eq)continue;*eq++=0;trim(line);trim(eq);
      if(!stricmp(line,"suiteTitle")){if(*eq){strncpy(suite_title,eq,sizeof(suite_title)-1);suite_title[sizeof(suite_title)-1]=0;}}
      else if(!stricmp(line,"MAIN_TITLE")){v=atoi(eq);if(v>=0&&v<=15)win_accent_index=v;}
      else if(!stricmp(line,"LAUNCHERS")){v=atoi(eq);if(v>=0&&v<=15)win_launcher_index=v;}
    }
    fclose(f);
  }
  button_x=(int)GetPrivateProfileInt("Button","X",24,win16_ini_path);
  button_edge=(int)GetPrivateProfileInt("Button","Edge",0,win16_ini_path)?1:0;
}

static void save_button_position(void)
{
  char value[16];
  sprintf(value,"%d",button_x);
  WritePrivateProfileString("Button","X",value,win16_ini_path);
  sprintf(value,"%d",button_edge);
  WritePrivateProfileString("Button","Edge",value,win16_ini_path);
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

static int find_folder_node(int section_index,const char *title)
{
  int i;
  for(i=0;i<node_count;i++)
    if(nodes[i].section_index==section_index && nodes[i].is_folder &&
       !stricmp(nodes[i].title,title))return i;
  return -1;
}

static int ensure_folder_node(int section_index,const char *title,int child_section_index)
{
  int i=find_folder_node(section_index,title);MENU_NODE *n;
  if(i>=0){nodes[i].child_section_index=child_section_index;return i;}
  if(node_count>=MAX_NODES)return -1;
  n=&nodes[node_count++];memset(n,0,sizeof(*n));n->is_folder=1;
  strncpy(n->title,title,sizeof(n->title)-1);n->title[sizeof(n->title)-1]=0;trim(n->title);
  n->section_index=section_index;n->child_section_index=child_section_index;
  return node_count-1;
}

/* Match the DOS menu loader's useful tolerance for section-only folder trees.
   A long-lived LAUNCH.MNU may contain [Launcher\Folder] sections without a
   separate FOLDER=Folder record at the parent.  DOS reconstructs that parent
   automatically; Win16 must do the same or the native root menu appears empty. */
static int ensure_section_tree(const char *name)
{
  char work[128],full[128],*p,*q;int parent,child;
  strncpy(work,name,sizeof(work)-1);work[sizeof(work)-1]=0;trim(work);
  if(strnicmp(work,"Launcher",8) || (work[8] && work[8]!='\\'))return ensure_section(work);
  strcpy(full,"Launcher");parent=ensure_section(full);if(parent<0)return -1;
  p=work+8;if(*p=='\\')p++;
  while(*p){
    q=strchr(p,'\\');if(q)*q=0;trim(p);if(!*p)return -1;
    if(strlen(full)+strlen(p)+2>=sizeof(full))return -1;
    strcat(full,"\\");strcat(full,p);
    child=ensure_section(full);if(child<0)return -1;
    if(ensure_folder_node(parent,p,child)<0)return -1;
    parent=child;if(!q)break;p=q+1;
  }
  return parent;
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
    if(line[0]=='['){char *r=strchr(line,']');if(r){*r=0;strncpy(section,line+1,sizeof(section)-1);section[sizeof(section)-1]=0;trim(section);section_index=ensure_section_tree(section);}continue;}
    if(node_count>=MAX_NODES)continue;
    if(!strnicmp(line,"FOLDER=",7)){
      char title[48],child[128];int child_index;
      strncpy(title,line+7,sizeof(title)-1);title[sizeof(title)-1]=0;trim(title);
      if(!title[0] || section_index<0)continue;
      if(strlen(section)+strlen(title)+2>=sizeof(child))continue;
      strcpy(child,section);strcat(child,"\\");strcat(child,title);
      child_index=ensure_section(child);
      if(child_index>=0)ensure_folder_node(section_index,title,child_index);
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

static int section_has_nodes(int section_index)
{
  int i;
  for(i=0;i<node_count;i++)if(nodes[i].section_index==section_index)return 1;
  return 0;
}

static void build_native_menu(void)
{
  int i;HMENU m,sub;
  /* Create a fresh native USER menu tree for each invocation. */
  for(i=0;i<section_count;i++)sections[i].menu=CreatePopupMenu();
  for(i=0;i<node_count;i++){
    m=(nodes[i].section_index>=0&&nodes[i].section_index<section_count)?sections[nodes[i].section_index].menu:NULL;if(!m)continue;
    if(!strcmp(nodes[i].title,"-")){
      if(manage_mode==MANAGE_REMOVE)AppendMenu(m,MF_STRING,IDM_REMOVE_FIRST+i,"[Separator]");
      else AppendMenu(m,MF_SEPARATOR,0,NULL);
      continue;
    }
    if(nodes[i].is_folder){
      sub=(nodes[i].child_section_index>=0&&nodes[i].child_section_index<section_count)?sections[nodes[i].child_section_index].menu:NULL;
      if(sub){
        if(manage_mode==MANAGE_EDIT){
          AppendMenu(sub,MF_STRING,IDM_EDIT_FIRST+i,"Edit this folder...");
          if(section_has_nodes(nodes[i].child_section_index))AppendMenu(sub,MF_SEPARATOR,0,NULL);
        } else if(manage_mode==MANAGE_REMOVE){
          AppendMenu(sub,MF_STRING,IDM_REMOVE_FIRST+i,"Remove this folder...");
          if(section_has_nodes(nodes[i].child_section_index))AppendMenu(sub,MF_SEPARATOR,0,NULL);
        }
        AppendMenu(m,MF_POPUP,(UINT)sub,nodes[i].title);
      }
    }
    else {
      char label[52];UINT id;
      strncpy(label,nodes[i].title,sizeof(label)-1);label[sizeof(label)-1]=0;
      id=(manage_mode==MANAGE_EDIT)?(IDM_EDIT_FIRST+i):
         (manage_mode==MANAGE_REMOVE)?(IDM_REMOVE_FIRST+i):(IDM_FIRST+i);
      AppendMenu(m,MF_STRING,id,label);
    }
  }
  root_menu=menu_for_section("Launcher");
  if(root_menu && manage_mode==MANAGE_NONE){
    AppendMenu(root_menu,MF_SEPARATOR,0,NULL);
    AppendMenu(root_menu,MF_STRING,IDM_RUN,"&Run...");
    AppendMenu(root_menu,MF_STRING,IDM_EXPLORE,"&Explore");
    AppendMenu(root_menu,MF_STRING,IDM_EXITWIN,"E&xit Windows");
  }
}

static void first_token(const char *cmd,char *out,int max)
{
  int i=0;const char *p=cmd;while(*p==' '||*p=='\t')p++;
  while(*p&&*p!=' '&&*p!='\t'&&i<max-1)out[i++]=*p++;
  out[i]=0;
}

static int is_windows_exe(const char *cmd)
{
  char token[144],path[160],search[160];FILE *f;unsigned char sig[2],os;long neoff;unsigned char b[4];
  first_token(cmd,token,sizeof(token));if(!token[0])return 0;
  strcpy(path,token);f=fopen(path,"rb");
  if(!f && !strchr(path,'\\') && !strchr(path,':')){
    sprintf(path,"%s\\%s",base_dir,token);f=fopen(path,"rb");
    if(!f){search[0]=0;_searchenv(token,"PATH",search);if(search[0]){strcpy(path,search);f=fopen(path,"rb");}}
  }
  if(!f){char wdir[144];GetWindowsDirectory(wdir,sizeof(wdir));sprintf(path,"%s\\%s",wdir,token);f=fopen(path,"rb");}
  if(!f)return 0;
  if(fread(sig,1,2,f)!=2||sig[0]!='M'||sig[1]!='Z'){fclose(f);return 0;}
  if(fseek(f,0x3c,SEEK_SET)||fread(b,1,4,f)!=4){fclose(f);return 0;}
  neoff=(long)b[0]|((long)b[1]<<8)|((long)b[2]<<16)|((long)b[3]<<24);
  if(fseek(f,neoff,SEEK_SET)||fread(sig,1,2,f)!=2||sig[0]!='N'||sig[1]!='E'){fclose(f);return 0;}
  if(fseek(f,neoff+0x36L,SEEK_SET)||fread(&os,1,1,f)!=1){fclose(f);return 0;}fclose(f);
  return os==2||os==4;
}
static int item_command_needs_file_validation(const char *cmd)
{
  char token[144];char *dot;first_token(cmd,token,sizeof(token));dot=strrchr(token,'.');
  if(!dot)return 0;
  return !stricmp(dot,".EXE")||!stricmp(dot,".COM")||!stricmp(dot,".BAT");
}

static int item_command_file_exists(const char *cmd)
{
  char token[144],path[160],search[160];FILE *f;
  first_token(cmd,token,sizeof(token));if(!token[0])return 0;
  strcpy(path,token);f=fopen(path,"rb");
  if(!f && !strchr(path,'\\') && !strchr(path,':')){
    sprintf(path,"%s\\%s",base_dir,token);f=fopen(path,"rb");
    if(!f){search[0]=0;_searchenv(token,"PATH",search);if(search[0]){strcpy(path,search);f=fopen(path,"rb");}}
  }
  if(!f){char wdir[144];GetWindowsDirectory(wdir,sizeof(wdir));sprintf(path,"%s\\%s",wdir,token);f=fopen(path,"rb");}
  if(!f)return 0;fclose(f);return 1;
}

static int item_command_validation(const char *cmd)
{
  if(!item_command_needs_file_validation(cmd))return -1;
  return item_command_file_exists(cmd)?1:0;
}

static COLORREF launch_colourref(int index)
{
  static const unsigned char rgb[16][3]={
    {0,0,0},{0,0,170},{0,170,0},{0,170,170},{170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},{255,85,85},{255,85,255},{255,255,85},{255,255,255}
  };
  if(index<0||index>15)index=7;return RGB(rgb[index][0],rgb[index][1],rgb[index][2]);
}

static void run_dos(const char *cmd)
{
  FILE *f;char shell[220];
  f=fopen(winrun_path,"wt");if(!f){show_message_dialog(gWnd,"Launch!","Could not create WINRUN.BAT.",MSG_ERROR);return;}
  fprintf(f,"@ECHO OFF\n%s\nPAUSE\n",cmd);fclose(f);
  sprintf(shell,"COMMAND.COM /C %s",winrun_path);
  if(WinExec(shell,SW_SHOWNORMAL)<32)show_message_dialog(gWnd,"Launch!","Windows could not start the DOS command.",MSG_ERROR);
}

static int command_is_pif(const char *cmd)
{
  char token[144],*dot;first_token(cmd,token,sizeof(token));dot=strrchr(token,'.');
  return dot && !stricmp(dot,".PIF");
}

static void run_item(int idx)
{
  if(idx<0||idx>=node_count||nodes[idx].is_folder)return;
  if(is_windows_exe(nodes[idx].command)||command_is_pif(nodes[idx].command)){
    if(WinExec(nodes[idx].command,SW_SHOWNORMAL)<32)show_message_dialog(gWnd,"Launch!","Windows could not start this program.",MSG_ERROR);
  } else run_dos(nodes[idx].command);
}

static int wait_for_dde_flag(int *flag,DWORD timeout_ms)
{
  DWORD start=GetTickCount();MSG msg;
  while(*flag){
    while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
      TranslateMessage(&msg);DispatchMessage(&msg);
      if(!*flag)break;
    }
    if(!*flag)break;
    if((DWORD)(GetTickCount()-start)>=timeout_ms)break;
    Yield();
  }
  return *flag?0:1;
}

static int dde_connect_progman(void)
{
  HWND progman;ATOM app,topic;
  if(dde_server && IsWindow(dde_server))return 1;
  dde_server=NULL;progman=FindWindow("Progman",NULL);if(!progman)return 0;
  app=GlobalAddAtom("PROGMAN");topic=GlobalAddAtom("PROGMAN");
  if(!app||!topic){if(app)GlobalDeleteAtom(app);if(topic)GlobalDeleteAtom(topic);return 0;}
  dde_initiating=1;
  SendMessage(progman,WM_DDE_INITIATE,(WPARAM)gWnd,MAKELONG(app,topic));
  GlobalDeleteAtom(app);GlobalDeleteAtom(topic);
  if(dde_initiating)wait_for_dde_flag(&dde_initiating,1500L);
  return dde_server?1:0;
}

static HGLOBAL dde_request_text(const char *item)
{
  ATOM atom;
  if(!dde_connect_progman())return NULL;
  if(dde_reply){GlobalFree(dde_reply);dde_reply=NULL;}
  dde_request_ok=0;dde_waiting=1;
  atom=GlobalAddAtom(item);if(!atom){dde_waiting=0;return NULL;}
  if(!PostMessage(dde_server,WM_DDE_REQUEST,(WPARAM)gWnd,MAKELONG(CF_TEXT,atom))){
    GlobalDeleteAtom(atom);dde_waiting=0;return NULL;
  }
  if(!wait_for_dde_flag(&dde_waiting,5000L)){dde_waiting=0;return NULL;}
  if(!dde_request_ok){if(dde_reply){GlobalFree(dde_reply);dde_reply=NULL;}return NULL;}
  return dde_reply;
}

static void dde_disconnect_progman(void)
{
  HWND server=dde_server;
  dde_server=NULL;dde_waiting=0;dde_initiating=0;
  if(server && IsWindow(server))PostMessage(server,WM_DDE_TERMINATE,(WPARAM)gWnd,0L);
}

static void clean_menu_text(char *s)
{
  char *p=s;
  while(*p){if(*p=='|'||*p=='\r'||*p=='\n')*p=' ';p++;}
  trim(s);
}

static char *next_line_text(char **cursor)
{
  char *p=*cursor,*line,*end;
  if(!p||!*p)return NULL;line=p;end=p;
  while(*end&&*end!='\r'&&*end!='\n')end++;
  if(*end){*end++=0;if(*end=='\n'||*end=='\r')end++;}
  *cursor=end;return line;
}

static int csv_next_field(char **cursor,char *out,int max)
{
  char *p=*cursor;int n=0,quoted=0;
  if(!p||!*p){out[0]=0;return 0;}
  while(*p==' '||*p=='\t')p++;
  if(*p=='\"'){quoted=1;p++;}
  while(*p){
    if(quoted){
      if(*p=='\"'){
        if(p[1]=='\"'){if(n<max-1)out[n++]='\"';p+=2;continue;}
        p++;while(*p==' '||*p=='\t')p++;break;
      }
    } else if(*p==',')break;
    if(n<max-1)out[n++]=*p;p++;
  }
  out[n]=0;if(*p==',')p++;*cursor=p;trim(out);return 1;
}

static int write_progman_group(FILE *f,const char *group)
{
  HGLOBAL h;LPSTR data;char *cursor,*line;char title[96],command[192];int first=1,count=0;
  h=dde_request_text(group);if(!h)return 0;
  data=(LPSTR)GlobalLock(h);if(!data){GlobalFree(h);dde_reply=NULL;return 0;}
  cursor=data;
  while((line=next_line_text(&cursor))!=NULL){
    char *fields=line;
    if(first){first=0;continue;}
    title[0]=command[0]=0;
    if(!csv_next_field(&fields,title,sizeof(title)))continue;
    if(!csv_next_field(&fields,command,sizeof(command)))continue;
    clean_menu_text(title);clean_menu_text(command);
    if(!title[0]||!command[0])continue;
    if(fprintf(f,"ITEM=%s|%s|0|0|0|0\n",title,command)<0){GlobalUnlock(h);GlobalFree(h);dde_reply=NULL;return -1;}
    count++;
  }
  GlobalUnlock(h);GlobalFree(h);dde_reply=NULL;return count;
}

static int ensure_menu_file(void)
{
  FILE *f=fopen(menu_path,"rt");
  char msg[220];
  if(f){fclose(f);return 1;}
  /* !WIN16 never manufactures or searches for a menu.  The default menu is
     exactly LAUNCH.MNU beside !WIN16.EXE; an explicit command-line menu is
     the only override. */
  sprintf(msg,"Launch! could not open the menu file:\n\n%s",menu_path);
  show_message_dialog(gWnd,"Launch!",msg,MSG_ERROR);
  return 0;
}

static void begin_popup_tracking(HWND *previous_active,HWND *previous_focus)
{
  *previous_active=GetActiveWindow();*previous_focus=GetFocus();
  /* TrackPopupMenu on Windows 3.x needs a genuinely active/focused owner.
     The Launch! button normally uses SW_SHOWNOACTIVATE so it never steals
     focus from applications.  Temporarily promote it only while USER owns
     the modal popup loop, and suspend the 100 ms keep-on-top timer so no
     launcher-window housekeeping competes with menu mouse tracking. */
  menu_tracking=1;KillTimer(gWnd,RAISE_TIMER);
  if(!IsWindowEnabled(gWnd))EnableWindow(gWnd,TRUE);
  BringWindowToTop(gWnd);SetActiveWindow(gWnd);SetFocus(gButton?gButton:gWnd);UpdateWindow(gWnd);
}

static void end_popup_tracking(HWND previous_active,HWND previous_focus)
{
  menu_tracking=0;SetTimer(gWnd,RAISE_TIMER,100,NULL);
  if(previous_active && previous_active!=gWnd && IsWindow(previous_active)){
    SetActiveWindow(previous_active);
    if(previous_focus && IsWindow(previous_focus))SetFocus(previous_focus);
  }
  SetWindowPos(gWnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  InvalidateRect(gWnd,NULL,FALSE);UpdateWindow(gWnd);
}

/* TrackPopupMenu owns a private modal message loop.  Queue one navigation
   keystroke immediately before entering that loop so USER itself establishes
   the initial selection using normal menu rules (including skipping separators
   and disabled items).  Down selects the first item for a top-edge launcher;
   Up selects the last selectable item for a bottom-edge launcher. */
static void queue_initial_menu_focus(void)
{
  UINT key=button_edge?VK_UP:VK_DOWN;
  PostMessage(gWnd,WM_KEYDOWN,key,0L);
  PostMessage(gWnd,WM_KEYUP,key,0L);
}

static void show_launch_menu(void)
{
  POINT pt;RECT wr;UINT flags;int i;HWND previous_active,previous_focus;
  if(!ensure_menu_file())return;
  if(!load_launch_menu()){show_message_dialog(gWnd,"Launch!","The selected menu file could not be opened.",MSG_ERROR);return;}
  load_win_preferences();resize_button_for_mode();
  build_native_menu();if(!root_menu)return;
  begin_popup_tracking(&previous_active,&previous_focus);
  GetWindowRect(gWnd,&wr);pt.x=wr.left;
  flags=TPM_LEFTALIGN|TPM_LEFTBUTTON;
  if(button_edge){pt.y=wr.top;flags|=TPM_BOTTOMALIGN;}
  else pt.y=wr.bottom;
  queue_initial_menu_focus();
  TrackPopupMenu(root_menu,flags,pt.x,pt.y,0,gWnd,NULL);
  DestroyMenu(root_menu);root_menu=NULL;
  for(i=0;i<section_count;i++)sections[i].menu=NULL;
  end_popup_tracking(previous_active,previous_focus);
}


static int copy_file_local(const char *src,const char *dst)
{
  FILE *in,*out;char buf[1024];size_t n;int ok=1;
  in=fopen(src,"rb");if(!in)return 0;
  out=fopen(dst,"wb");if(!out){fclose(in);return 0;}
  while((n=fread(buf,1,sizeof(buf),in))!=0){if(fwrite(buf,1,n,out)!=n){ok=0;break;}}
  if(ferror(in))ok=0;
  fclose(in);if(fclose(out)!=0)ok=0;
  if(!ok)remove(dst);
  return ok;
}

static int write_menu_section(FILE *f,int section_index)
{
  int i;
  if(section_index<0||section_index>=section_count)return 0;
  if(fprintf(f,"[%s]\n",sections[section_index].name)<0)return 0;
  for(i=0;i<node_count;i++)if(nodes[i].section_index==section_index){
    if(!strcmp(nodes[i].title,"-")){
      if(fputs("SEPARATOR=\n",f)==EOF)return 0;
    } else if(nodes[i].is_folder){
      if(fprintf(f,"FOLDER=%s\n",nodes[i].title)<0)return 0;
    } else {
      if(fprintf(f,"ITEM=%s|%s|%d|%d|%d|%d\n",nodes[i].title,nodes[i].command,
                 nodes[i].enter_after,nodes[i].change_dir,nodes[i].prompt,nodes[i].add_path)<0)return 0;
    }
  }
  if(fputc('\n',f)==EOF)return 0;
  for(i=0;i<node_count;i++)if(nodes[i].section_index==section_index && nodes[i].is_folder){
    if(!write_menu_section(f,nodes[i].child_section_index))return 0;
  }
  return 1;
}

static int save_launch_menu(void)
{
  char tmp[180],bak[180];FILE *f;int root,ok;
  menu_sidecar_path(tmp,".$$$");menu_sidecar_path(bak,".BAK");
  root=find_section("Launcher");if(root<0)return 0;
  remove(tmp);f=fopen(tmp,"wt");if(!f)return 0;
  ok=fputs("; Launch! 3.76 menu definition\n; ITEM=title|command and parameters|press Enter|change directory|prompt|add to PATH (0/1)\n; SEPARATOR= adds a movable horizontal separator\n\n",f)!=EOF;
  if(ok)ok=write_menu_section(f,root);
  if(fclose(f)!=0)ok=0;
  if(!ok){remove(tmp);return 0;}
  remove(bak);copy_file_local(menu_path,bak);
  if(remove(menu_path)!=0){remove(tmp);return 0;}
  if(rename(tmp,menu_path)!=0){copy_file_local(bak,menu_path);remove(tmp);return 0;}
  if(!load_launch_menu())return 0;
  return 1;
}

static void split_command_win(const char *command,char *exe,char *params)
{
  const char *p=command,*end;int n;
  while(*p==' '||*p=='\t')p++;
  end=p;while(*end&&*end!=' '&&*end!='\t')end++;
  n=(int)(end-p);if(n>158)n=158;
  strncpy(exe,p,n);exe[n]=0;
  while(*end==' '||*end=='\t')end++;
  strncpy(params,end,159);params[159]=0;
}

static int folder_name_exists(int section_index,const char *name,int except_node)
{
  int i;
  for(i=0;i<node_count;i++)if(i!=except_node && nodes[i].section_index==section_index && nodes[i].is_folder && !stricmp(nodes[i].title,name))return 1;
  return 0;
}

static void rename_folder_sections(int node_index,const char *new_title)
{
  int i,child;char old_prefix[128],new_prefix[128],tail[128];int old_len;
  if(node_index<0||node_index>=node_count||!nodes[node_index].is_folder)return;
  child=nodes[node_index].child_section_index;if(child<0||child>=section_count)return;
  strcpy(old_prefix,sections[child].name);old_len=strlen(old_prefix);
  sprintf(new_prefix,"%s\\%s",sections[nodes[node_index].section_index].name,new_title);
  for(i=0;i<section_count;i++){
    if(!stricmp(sections[i].name,old_prefix)){
      strncpy(sections[i].name,new_prefix,sizeof(sections[i].name)-1);sections[i].name[sizeof(sections[i].name)-1]=0;
    } else if(!strnicmp(sections[i].name,old_prefix,old_len) && sections[i].name[old_len]=='\\'){
      strncpy(tail,sections[i].name+old_len,sizeof(tail)-1);tail[sizeof(tail)-1]=0;
      strncpy(sections[i].name,new_prefix,sizeof(sections[i].name)-1);sections[i].name[sizeof(sections[i].name)-1]=0;
      strncat(sections[i].name,tail,sizeof(sections[i].name)-strlen(sections[i].name)-1);
    }
  }
}

static void center_dialog(HWND h)
{
  RECT r;int w,hgt,x,y;
  GetWindowRect(h,&r);w=r.right-r.left;hgt=r.bottom-r.top;
  x=(GetSystemMetrics(SM_CXSCREEN)-w)/2;y=(GetSystemMetrics(SM_CYSCREEN)-hgt)/2;
  if(x<0)x=0;if(y<0)y=0;
  SetWindowPos(h,NULL,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER);
}

static BOOL FAR PASCAL ItemDlgProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  switch(msg){
    case WM_INITDIALOG:
      itemWnd=h;center_dialog(h);
      SetWindowText(h,item_dialog_folder?(item_dialog_editing?"Edit Folder":"Add Folder"):(item_dialog_editing?"Edit Launcher":"Add Launcher"));
      itemName=GetDlgItem(h,ITEM_NAME);
      itemCommand=item_dialog_folder?NULL:GetDlgItem(h,ITEM_COMMAND);
      itemParams=item_dialog_folder?NULL:GetDlgItem(h,ITEM_PARAMS);
      itemPrompt=item_dialog_folder?NULL:GetDlgItem(h,ITEM_PROMPT);
      itemEnter=item_dialog_folder?NULL:GetDlgItem(h,ITEM_ENTER);
      itemChdir=item_dialog_folder?NULL:GetDlgItem(h,ITEM_CHDIR);
      itemAddpath=item_dialog_folder?NULL:GetDlgItem(h,ITEM_ADDPATH);
      SendMessage(itemName,EM_LIMITTEXT,item_dialog_folder?16:18,0L);
      SetWindowText(itemName,item_work.title);
      if(!item_dialog_folder){
        SendMessage(itemCommand,EM_LIMITTEXT,158,0L);
        SendMessage(itemParams,EM_LIMITTEXT,158,0L);
        SetWindowText(itemCommand,item_exe);SetWindowText(itemParams,item_params);
        SendMessage(itemPrompt,BM_SETCHECK,item_work.prompt?1:0,0L);
        SendMessage(itemEnter,BM_SETCHECK,item_work.enter_after?1:0,0L);
        SendMessage(itemChdir,BM_SETCHECK,item_work.change_dir?1:0,0L);
        SendMessage(itemAddpath,BM_SETCHECK,item_work.add_path?1:0,0L);
      }
      SetFocus(itemName);return FALSE;
    case WM_COMMAND:
      if(wp==ITEM_COMMAND && HIWORD(lp)==EN_CHANGE){InvalidateRect(itemCommand,NULL,TRUE);UpdateWindow(itemCommand);return TRUE;}
      if(wp==ITEM_BROWSE){
        char file[160]="";
        if(browse_for_program(h,file,sizeof(file)))SetWindowText(itemCommand,file);
        return TRUE;
      }
      if(wp==IDOK){
        char name[48],exe[160],params[160],cmd[160];
        GetWindowText(itemName,name,sizeof(name));trim(name);
        if(!name[0]){show_message_dialog(h,item_dialog_folder?"Folder":"Launcher","Enter a name.",MSG_WARNING);SetFocus(itemName);return TRUE;}
        if(item_dialog_folder){
          if(folder_name_exists(item_dialog_section,name,item_dialog_node)){show_message_dialog(h,"Folder","That folder name already exists in this menu.",MSG_WARNING);return TRUE;}
          strncpy(item_work.title,name,sizeof(item_work.title)-1);item_work.title[sizeof(item_work.title)-1]=0;
        } else {
          GetWindowText(itemCommand,exe,sizeof(exe));trim(exe);GetWindowText(itemParams,params,sizeof(params));trim(params);
          if(!exe[0]){show_message_dialog(h,"Launcher","Enter a command.",MSG_WARNING);SetFocus(itemCommand);return TRUE;}
          strncpy(item_work.title,name,sizeof(item_work.title)-1);item_work.title[sizeof(item_work.title)-1]=0;
          strcpy(cmd,exe);if(params[0] && strlen(cmd)<sizeof(cmd)-2){strcat(cmd," ");strncat(cmd,params,sizeof(cmd)-strlen(cmd)-1);}
          strncpy(item_work.command,cmd,sizeof(item_work.command)-1);item_work.command[sizeof(item_work.command)-1]=0;
          item_work.prompt=SendMessage(itemPrompt,BM_GETCHECK,0,0L)?1:0;
          item_work.enter_after=SendMessage(itemEnter,BM_GETCHECK,0,0L)?1:0;
          item_work.change_dir=SendMessage(itemChdir,BM_GETCHECK,0,0L)?1:0;
          item_work.add_path=SendMessage(itemAddpath,BM_GETCHECK,0,0L)?1:0;
        }
        item_dialog_ok=1;EndDialog(h,IDOK);return TRUE;
      }
      if(wp==IDCANCEL){EndDialog(h,IDCANCEL);return TRUE;}
      break;
    case WM_CTLCOLOR:
      if(!item_dialog_folder && (HWND)LOWORD(lp)==itemCommand){
        char exe[160];int valid;GetWindowText(itemCommand,exe,sizeof(exe));trim(exe);valid=item_command_validation(exe);
        if(valid>=0){
          SetTextColor((HDC)wp,launch_colourref(valid?win_launcher_index:win_accent_index));
          SetBkColor((HDC)wp,GetSysColor(COLOR_WINDOW));return (BOOL)gWindowBrush;
        }
      }
      break;
    case WM_CLOSE:EndDialog(h,IDCANCEL);return TRUE;
    case WM_DESTROY:itemWnd=NULL;return TRUE;
  }
  return FALSE;
}

static int run_item_dialog(int node_index,int folder,int editing,int section_index)
{
  FARPROC proc;int rc;
  load_win_preferences();
  memset(&item_work,0,sizeof(item_work));item_exe[0]=item_params[0]=0;
  item_dialog_node=node_index;item_dialog_folder=folder;item_dialog_editing=editing;item_dialog_section=section_index;
  if(editing && node_index>=0 && node_index<node_count){item_work=nodes[node_index];if(!folder)split_command_win(item_work.command,item_exe,item_params);}
  else {item_work.enter_after=1;item_work.section_index=section_index;}
  item_dialog_ok=0;itemWnd=NULL;
  proc=MakeProcInstance((FARPROC)ItemDlgProc,gInst);if(!proc)return 0;
  menu_tracking=1;
  rc=DialogBox(gInst,MAKEINTRESOURCE(folder?IDD_FOLDER:IDD_LAUNCHER),gWnd,proc);
  menu_tracking=0;
  FreeProcInstance(proc);itemWnd=NULL;SetActiveWindow(gWnd);
  if(rc==-1){show_message_dialog(gWnd,"Launch!","Could not create the Launch! dialog.",MSG_ERROR);return 0;}
  return item_dialog_ok;
}

static int add_menu_item(int folder)
{
  int root;MENU_NODE *n;char child[128];
  root=find_section("Launcher");if(root<0)return 0;
  if(node_count>=MAX_NODES){show_message_dialog(gWnd,"Launch!","The menu is full.",MSG_WARNING);return 0;}
  if(!run_item_dialog(-1,folder,0,root))return 0;
  n=&nodes[node_count];*n=item_work;n->section_index=root;n->is_folder=folder;n->child_section_index=-1;
  if(folder){
    if(section_count>=MAX_SECTIONS){show_message_dialog(gWnd,"Launch!","The menu has too many folders.",MSG_WARNING);return 0;}
    sprintf(child,"%s\\%s",sections[root].name,n->title);n->child_section_index=ensure_section(child);
  }
  node_count++;
  if(!save_launch_menu()){show_message_dialog(gWnd,"Launch!","Could not update the selected menu file.",MSG_ERROR);return 0;}
  return 1;
}

static int add_menu_separator(void)
{
  int root;MENU_NODE *n;
  root=find_section("Launcher");if(root<0||node_count>=MAX_NODES)return 0;
  n=&nodes[node_count++];memset(n,0,sizeof(*n));strcpy(n->title,"-");n->section_index=root;n->child_section_index=-1;
  if(!save_launch_menu()){show_message_dialog(gWnd,"Launch!","Could not update the selected menu file.",MSG_ERROR);return 0;}
  return 1;
}

static void edit_menu_node(int node_index)
{
  char old_title[48];
  if(node_index<0||node_index>=node_count)return;
  if(!strcmp(nodes[node_index].title,"-"))return;
  strcpy(old_title,nodes[node_index].title);
  if(!run_item_dialog(node_index,nodes[node_index].is_folder,1,nodes[node_index].section_index))return;
  if(nodes[node_index].is_folder && stricmp(old_title,item_work.title))rename_folder_sections(node_index,item_work.title);
  item_work.section_index=nodes[node_index].section_index;
  item_work.child_section_index=nodes[node_index].child_section_index;
  item_work.is_folder=nodes[node_index].is_folder;
  nodes[node_index]=item_work;
  if(!save_launch_menu())show_message_dialog(gWnd,"Launch!","Could not update the selected menu file.",MSG_ERROR);
}

static int section_is_under(int section_index,const char *prefix)
{
  int n;if(section_index<0||section_index>=section_count)return 0;n=strlen(prefix);
  if(!stricmp(sections[section_index].name,prefix))return 1;
  return !strnicmp(sections[section_index].name,prefix,n) && sections[section_index].name[n]=='\\';
}

static void remove_menu_node(int node_index)
{
  int i,j=0;char msg[160],prefix[128];
  if(node_index<0||node_index>=node_count)return;
  if(nodes[node_index].is_folder)sprintf(msg,"Remove folder '%s' and all of its contents?",nodes[node_index].title);
  else if(!strcmp(nodes[node_index].title,"-"))strcpy(msg,"Remove this separator?");
  else sprintf(msg,"Remove '%s'?",nodes[node_index].title);
  if(!show_confirm_dialog("Remove Item",msg))return;
  prefix[0]=0;if(nodes[node_index].is_folder && nodes[node_index].child_section_index>=0)strcpy(prefix,sections[nodes[node_index].child_section_index].name);
  for(i=0;i<node_count;i++){
    if(i==node_index)continue;
    if(prefix[0] && section_is_under(nodes[i].section_index,prefix))continue;
    if(j!=i)nodes[j]=nodes[i];j++;
  }
  node_count=j;
  if(!save_launch_menu())show_message_dialog(gWnd,"Launch!","Could not update the selected menu file.",MSG_ERROR);
}

static void reorder_add_section(HWND list,int section_index,int depth)
{
  int i,j;char label[96];
  for(i=0;i<node_count;i++)if(nodes[i].section_index==section_index){
    label[0]=0;for(j=0;j<depth && (int)strlen(label)<20;j++)strcat(label,"  ");
    if(nodes[i].is_folder){strcat(label,"[Folder] ");strncat(label,nodes[i].title,sizeof(label)-strlen(label)-1);}
    else if(!strcmp(nodes[i].title,"-"))strcat(label,"----------");
    else strncat(label,nodes[i].title,sizeof(label)-strlen(label)-1);
    SendMessage(list,LB_ADDSTRING,0,(LPARAM)(LPSTR)label);if(reorder_rows<MAX_NODES)reorder_map[reorder_rows++]=i;
    if(nodes[i].is_folder)reorder_add_section(list,nodes[i].child_section_index,depth+1);
  }
}

static void refill_reorder_list(int select_node)
{
  int root,row=0;
  if(!reorderList)return;SendMessage(reorderList,LB_RESETCONTENT,0,0L);reorder_rows=0;
  root=find_section("Launcher");if(root>=0)reorder_add_section(reorderList,root,0);
  while(row<reorder_rows && reorder_map[row]!=select_node)row++;
  if(row<reorder_rows)SendMessage(reorderList,LB_SETCURSEL,row,0L);else if(reorder_rows)SendMessage(reorderList,LB_SETCURSEL,0,0L);
}

static int adjacent_same_section(int node_index,int direction)
{
  int i,section;if(node_index<0||node_index>=node_count)return -1;section=nodes[node_index].section_index;
  if(direction<0){for(i=node_index-1;i>=0;i--)if(nodes[i].section_index==section)return i;}
  else {for(i=node_index+1;i<node_count;i++)if(nodes[i].section_index==section)return i;}
  return -1;
}

static void reorder_move(int direction)
{
  int row,node,other;MENU_NODE tmp;
  row=(int)SendMessage(reorderList,LB_GETCURSEL,0,0L);if(row==LB_ERR||row<0||row>=reorder_rows)return;
  node=reorder_map[row];other=adjacent_same_section(node,direction);if(other<0)return;
  tmp=nodes[node];nodes[node]=nodes[other];nodes[other]=tmp;
  if(!save_launch_menu()){show_message_dialog(reorderWnd,"Launch!","Could not update the selected menu file.",MSG_ERROR);return;}
  /* Reload compacts section state; find the moved item by its former array position after the swap. */
  refill_reorder_list(other);
}

static LRESULT FAR PASCAL ReorderProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  switch(msg){
    case WM_CREATE:{
      HWND up,down,close;
      reorderList=CreateWindow("LISTBOX","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY,8,8,250,210,h,(HMENU)REORDER_LIST,gInst,NULL);
      up=CreateWindow("BUTTON","&Up",WS_CHILD|WS_VISIBLE|WS_TABSTOP,268,20,64,24,h,(HMENU)REORDER_UP,gInst,NULL);
      down=CreateWindow("BUTTON","&Down",WS_CHILD|WS_VISIBLE|WS_TABSTOP,268,52,64,24,h,(HMENU)REORDER_DOWN,gInst,NULL);
      close=CreateWindow("BUTTON","Close",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,268,194,64,24,h,(HMENU)REORDER_CLOSE,gInst,NULL);
      if(gDialogFont){SendMessage(reorderList,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(up,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(down,WM_SETFONT,(WPARAM)gDialogFont,0);SendMessage(close,WM_SETFONT,(WPARAM)gDialogFont,0);}
      refill_reorder_list(-1);return 0;}
    case WM_COMMAND:
      if(wp==REORDER_UP){reorder_move(-1);return 0;}
      if(wp==REORDER_DOWN){reorder_move(1);return 0;}
      if(wp==REORDER_CLOSE){reorder_done=1;DestroyWindow(h);return 0;}
      break;
    case WM_CLOSE:reorder_done=1;DestroyWindow(h);return 0;
    case WM_DESTROY:reorderWnd=NULL;reorderList=NULL;return 0;
  }
  return DefWindowProc(h,msg,wp,lp);
}

static void show_reorder_dialog(void)
{
  MSG msg;reorder_done=0;
  if(!ensure_menu_file())return;
  if(!load_launch_menu()){show_message_dialog(gWnd,"Launch!","The selected menu file could not be opened.",MSG_ERROR);return;}
  reorderWnd=CreateWindow("LaunchReorderDialog","Re-order Menu",WS_POPUP|WS_CAPTION|WS_SYSMENU,120,70,344,254,gWnd,NULL,gInst,NULL);
  if(!reorderWnd)return;
  menu_tracking=1;EnableWindow(gWnd,FALSE);ShowWindow(reorderWnd,SW_SHOW);UpdateWindow(reorderWnd);
  while(!reorder_done && GetMessage(&msg,NULL,0,0)){
    if(!IsDialogMessage(reorderWnd,&msg)){TranslateMessage(&msg);DispatchMessage(&msg);}
  }
  EnableWindow(gWnd,TRUE);menu_tracking=0;SetActiveWindow(gWnd);
}

static void show_management_menu(void)
{
  HMENU menu,add;POINT pt;RECT wr;UINT flags;HWND previous_active,previous_focus;
  manage_mode=MANAGE_MENU;resize_button_for_mode();
  menu=CreatePopupMenu();add=CreatePopupMenu();
  if(!menu||!add){
    if(menu)DestroyMenu(menu);if(add)DestroyMenu(add);
    manage_mode=MANAGE_NONE;resize_button_for_mode();return;
  }
  AppendMenu(add,MF_STRING,IDM_MGMT_ADD_LAUNCHER,"Launcher");
  AppendMenu(add,MF_STRING,IDM_MGMT_ADD_FOLDER,"Folder");
  AppendMenu(add,MF_STRING,IDM_MGMT_ADD_SEPARATOR,"Separator");
  AppendMenu(menu,MF_POPUP,(UINT)add,"Add");
  AppendMenu(menu,MF_STRING,IDM_MGMT_EDIT,"Edit");
  AppendMenu(menu,MF_STRING,IDM_MGMT_REMOVE,"Remove");
  AppendMenu(menu,MF_STRING,IDM_MGMT_REORDER,"Re-order");
  begin_popup_tracking(&previous_active,&previous_focus);
  GetWindowRect(gWnd,&wr);pt.x=wr.left;flags=TPM_LEFTALIGN|TPM_LEFTBUTTON;
  if(button_edge){pt.y=wr.top;flags|=TPM_BOTTOMALIGN;}else pt.y=wr.bottom;
  queue_initial_menu_focus();
  TrackPopupMenu(menu,flags,pt.x,pt.y,0,gWnd,NULL);
  DestroyMenu(menu);
  end_popup_tracking(previous_active,previous_focus);
  manage_mode=MANAGE_NONE;resize_button_for_mode();
}

static void begin_manage_mode(int mode)
{
  if(menu_tracking)return;
  manage_mode=mode;resize_button_for_mode();
  show_launch_menu();
  manage_mode=MANAGE_NONE;resize_button_for_mode();
}

static const char *current_button_text(void)
{
  if(manage_mode==MANAGE_EDIT)return "Edit...";
  if(manage_mode==MANAGE_REMOVE)return "Remove...";
  if(manage_mode==MANAGE_MENU)return "Manage";
  return suite_title;
}

static void button_size(int *bw,int *bh)
{
  int cw=GetSystemMetrics(SM_CXSIZE),ch=GetSystemMetrics(SM_CYSIZE);
  int tw=0;HDC dc;HFONT oldf=NULL;DWORD ext;const char *label=current_button_text();
  dc=GetDC(NULL);
  if(dc){
    if(gButtonFont)oldf=(HFONT)SelectObject(dc,gButtonFont);
    ext=GetTextExtent(dc,label,strlen(label));tw=(int)LOWORD(ext);
    if(oldf)SelectObject(dc,oldf);
    ReleaseDC(NULL,dc);
  }
  *bw=tw+14;if(*bw<cw+8)*bw=cw+8;
  *bh=ch>18?ch:18;
}

static void resize_button_for_mode(void)
{
  int bw,bh,sw,sh,by;
  if(!gWnd)return;
  if(gButton)SetWindowText(gButton,current_button_text());
  button_size(&bw,&bh);
  sw=GetSystemMetrics(SM_CXSCREEN);sh=GetSystemMetrics(SM_CYSCREEN);
  if(button_x<0)button_x=0;if(button_x>sw-bw)button_x=sw-bw;
  by=button_edge?sh-bh:0;
  SetWindowPos(gWnd,HWND_TOP,button_x,by,bw,bh,SWP_NOACTIVATE);
  if(gButton){MoveWindow(gButton,0,0,bw,bh,TRUE);InvalidateRect(gButton,NULL,TRUE);UpdateWindow(gButton);}
}

static void raise_button(HWND h)
{
  /* Z-order only: never change the canonical (24,0) geometry here. */
  SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
}

static int execute_run_dialog(HWND h)
{
  char cmd[256];int show=IsDlgButtonChecked(h,RUN_MIN)?SW_SHOWMINIMIZED:SW_SHOWNORMAL;
  GetDlgItemText(h,RUN_EDIT,cmd,sizeof(cmd));
  if(!cmd[0])return 0;
  if(WinExec(cmd,show)<32){show_message_dialog(h,"Run","Windows could not run this command.",MSG_ERROR);return 0;}
  return 1;
}

static int browse_for_program(HWND owner,char *file,int maxfile)
{
  HINSTANCE lib;FARPROC proc;OPENFILENAME of;BOOL ok;
  lib=LoadLibrary("COMMDLG.DLL");
  if((UINT)lib<32){
    show_message_dialog(owner,"Launch!","Browse requires the Windows common-dialog library.",MSG_INFO);
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

static BOOL FAR PASCAL RunDlgProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  switch(msg){
    case WM_INITDIALOG:
      runWnd=h;center_dialog(h);runEdit=GetDlgItem(h,RUN_EDIT);runMin=GetDlgItem(h,RUN_MIN);
      SetFocus(runEdit);return FALSE;
    case WM_COMMAND:
      if(wp==IDOK){if(execute_run_dialog(h))EndDialog(h,IDOK);return TRUE;}
      if(wp==IDCANCEL){EndDialog(h,IDCANCEL);return TRUE;}
      if(wp==RUN_BROWSE){char file[160]="";if(browse_for_program(h,file,sizeof(file)))SetWindowText(runEdit,file);return TRUE;}
      break;
    case WM_CLOSE:EndDialog(h,IDCANCEL);return TRUE;
    case WM_DESTROY:runWnd=NULL;return TRUE;
  }
  return FALSE;
}

static void show_run_dialog(void)
{
  FARPROC proc;int rc;
  if(runWnd){SetActiveWindow(runWnd);return;}
  proc=MakeProcInstance((FARPROC)RunDlgProc,gInst);if(!proc)return;
  menu_tracking=1;
  rc=DialogBox(gInst,MAKEINTRESOURCE(IDD_RUN),gWnd,proc);
  menu_tracking=0;
  FreeProcInstance(proc);runWnd=NULL;SetActiveWindow(gWnd);
  if(rc==-1)show_message_dialog(gWnd,"Launch!","Could not create the Run dialog.",MSG_ERROR);
}

static BOOL FAR PASCAL ConfirmDlgProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  (void)lp;
  switch(msg){
    case WM_INITDIALOG:
      { HICON icon;
        center_dialog(h);SetWindowText(h,confirm_title);SetDlgItemText(h,CONFIRM_TEXT,confirm_text);
        icon=LoadIcon(gInst,MAKEINTRESOURCE(confirm_dialog_id==IDD_EXITWIN?IDI_SLEEP:IDI_CONFIRM));
        if(icon&&GetDlgItem(h,CONFIRM_ICON))SendDlgItemMessage(h,CONFIRM_ICON,STM_SETICON,(WPARAM)icon,0L);
      }
      SetFocus(GetDlgItem(h,IDNO));return FALSE;
    case WM_COMMAND:
      if(wp==IDYES){EndDialog(h,IDYES);return TRUE;}
      if(wp==IDNO||wp==IDCANCEL){EndDialog(h,IDNO);return TRUE;}
      break;
    case WM_CLOSE:EndDialog(h,IDNO);return TRUE;
  }
  return FALSE;
}

static BOOL FAR PASCAL MessageDlgProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  HICON icon;
  (void)lp;
  switch(msg){
    case WM_INITDIALOG:
      center_dialog(h);SetWindowText(h,message_title);SetDlgItemText(h,MESSAGE_TEXT,message_text);
      if(message_kind==MSG_ERROR)icon=LoadIcon(gInst,MAKEINTRESOURCE(IDI_ERROR));
      else if(message_kind==MSG_WARNING)icon=LoadIcon(NULL,IDI_EXCLAMATION);
      else icon=LoadIcon(gInst,MAKEINTRESOURCE(IDI_SUCCESS));
      if(icon)SendDlgItemMessage(h,MESSAGE_ICON,STM_SETICON,(WPARAM)icon,0L);
      SetFocus(GetDlgItem(h,IDOK));return FALSE;
    case WM_COMMAND:
      if(wp==IDOK||wp==IDCANCEL){EndDialog(h,IDOK);return TRUE;}
      break;
    case WM_CLOSE:EndDialog(h,IDOK);return TRUE;
  }
  return FALSE;
}

static void show_message_dialog(HWND owner,const char *title,const char *text,int kind)
{
  FARPROC proc;int rc;UINT mbflags=MB_OK;
  if(!owner)owner=gWnd;
  strncpy(message_title,title,sizeof(message_title)-1);message_title[sizeof(message_title)-1]=0;
  strncpy(message_text,text,sizeof(message_text)-1);message_text[sizeof(message_text)-1]=0;
  message_kind=kind;
  proc=MakeProcInstance((FARPROC)MessageDlgProc,gInst);
  if(!proc){
    if(kind==MSG_ERROR)mbflags|=MB_ICONSTOP;
    else if(kind==MSG_WARNING)mbflags|=MB_ICONEXCLAMATION;
    else mbflags|=MB_ICONINFORMATION;
    MessageBox(owner,text,title,mbflags);return;
  }
  rc=DialogBox(gInst,MAKEINTRESOURCE(IDD_MESSAGE),owner,proc);
  FreeProcInstance(proc);
  if(rc==-1){
    if(kind==MSG_ERROR)mbflags|=MB_ICONSTOP;
    else if(kind==MSG_WARNING)mbflags|=MB_ICONEXCLAMATION;
    else mbflags|=MB_ICONINFORMATION;
    MessageBox(owner,text,title,mbflags);
  }
}

static int show_confirm_dialog_resource(int dialog_id,const char *title,const char *text)
{
  FARPROC proc;int rc;
  strncpy(confirm_title,title,sizeof(confirm_title)-1);confirm_title[sizeof(confirm_title)-1]=0;
  strncpy(confirm_text,text,sizeof(confirm_text)-1);confirm_text[sizeof(confirm_text)-1]=0;
  proc=MakeProcInstance((FARPROC)ConfirmDlgProc,gInst);if(!proc)return 0;
  confirm_dialog_id=dialog_id;
  menu_tracking=1;rc=DialogBox(gInst,MAKEINTRESOURCE(dialog_id),gWnd,proc);menu_tracking=0;
  FreeProcInstance(proc);SetActiveWindow(gWnd);
  if(rc==-1){MessageBox(gWnd,"Could not create the confirmation dialog.","Launch!",MB_OK|MB_ICONSTOP);return 0;}
  return rc==IDYES;
}

static int show_confirm_dialog(const char *title,const char *text)
{
  return show_confirm_dialog_resource(IDD_CONFIRM,title,text);
}

static int show_exit_windows_dialog(void)
{
  return show_confirm_dialog_resource(IDD_EXITWIN,"Exit Windows","Exit Windows and return to the DOS prompt?");
}

static LRESULT FAR PASCAL ButtonProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  if(msg==WM_RBUTTONUP){SendMessage(gWnd,WM_RBUTTONUP,wp,lp);return 0;}
  if(msg==WM_LBUTTONDOWN && GetKeyState(VK_MENU)<0){SendMessage(gWnd,WM_BEGIN_BUTTON_DRAG,0,0L);return 0;}
  return CallWindowProc(gButtonProc,h,msg,wp,lp);
}

static LRESULT FAR PASCAL WndProc(HWND h,UINT msg,WPARAM wp,LPARAM lp)
{
  static int dragging=0;
  static int drag_grab_x=0;
  switch(msg){
    case WM_DDE_ACK:
      if(dde_initiating){dde_server=(HWND)wp;dde_initiating=0;return 0;}
      if(dde_waiting){ATOM a=(ATOM)HIWORD(lp);if(a)GlobalDeleteAtom(a);dde_request_ok=0;dde_waiting=0;return 0;}
      return 0;
    case WM_DDE_DATA:{
      HGLOBAL hData=(HGLOBAL)LOWORD(lp);ATOM a=(ATOM)HIWORD(lp);DDEDATA FAR *dd=NULL;int ack=0,release=0;
      if(hData)dd=(DDEDATA FAR*)GlobalLock(hData);
      if(dd && dd->cfFormat==CF_TEXT){
        DWORD len=(DWORD)lstrlen((LPSTR)dd->Value)+1L;LPSTR dst;HGLOBAL cp=GlobalAlloc(GMEM_MOVEABLE,len);
        ack=dd->fAckReq?1:0;release=dd->fRelease?1:0;
        if(cp && (dst=(LPSTR)GlobalLock(cp))!=NULL){lstrcpy(dst,(LPSTR)dd->Value);GlobalUnlock(cp);if(dde_reply)GlobalFree(dde_reply);dde_reply=cp;dde_request_ok=1;}
        else if(cp)GlobalFree(cp);
      }
      if(dd)GlobalUnlock(hData);
      if(ack && dde_server)PostMessage(dde_server,WM_DDE_ACK,(WPARAM)h,MAKELONG(0x8000,a));
      else if(a)GlobalDeleteAtom(a);
      if(release && hData)GlobalFree(hData);
      dde_waiting=0;return 0;}
    case WM_DDE_TERMINATE:
      if((HWND)wp==dde_server){HWND server=dde_server;dde_server=NULL;dde_waiting=0;dde_initiating=0;PostMessage(server,WM_DDE_TERMINATE,(WPARAM)h,0L);}return 0;
    case WM_CREATE:{
      RECT r;GetClientRect(h,&r);
      gButton=CreateWindow("BUTTON",suite_title,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,r.right-r.left,r.bottom-r.top,h,(HMENU)BTN_ID,gInst,NULL);
      if(!gButton)return -1;
      if(gButtonFont)SendMessage(gButton,WM_SETFONT,(WPARAM)gButtonFont,0L);
      gButtonProc=(FARPROC)SetWindowLong(gButton,GWL_WNDPROC,(LONG)(FARPROC)ButtonProc);
      SetTimer(h,RAISE_TIMER,100,NULL);return 0;}
    case WM_SHOW_LAUNCH_MENU:
      /* A helper invocation only schedules the resident popup.  It exits
         immediately; the resident opens the menu later from its own task,
         after Windows has completed the Program Manager/task activation
         hand-off.  This avoids the native popup being dismissed with the
         helper task. */
      if(wp){
        char path[160];path[0]=0;GlobalGetAtomName((ATOM)wp,path,sizeof(path));GlobalDeleteAtom((ATOM)wp);
        if(path[0]){strncpy(menu_path,path,sizeof(menu_path)-1);menu_path[sizeof(menu_path)-1]=0;}
      }
      if(!menu_tracking && !runWnd){menu_open_pending=1;menu_open_delay=5;}
      return 0;
    case WM_ACTIVATEAPP:
      /* Ordinary activation (including Alt+Tab) must never open the menu.
         Program Manager's Ctrl+Alt+\\ shortcut can activate an existing
         single-instance task while the actual shortcut keys are still down;
         accept only that exact key state as an invocation request. */
      if(wp && !menu_tracking && !runWnd &&
         GetKeyState(VK_CONTROL)<0 && GetKeyState(VK_MENU)<0 && GetKeyState(VK_OEM_5)<0){
        menu_open_pending=1;menu_open_delay=1;
      } else if(!wp && !menu_tracking){
        SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        InvalidateRect(h,NULL,FALSE);UpdateWindow(h);
      }
      return 0;
    case WM_SYSCOMMAND:
      /* Windows also reports an application hot key explicitly as SC_HOTKEY.
         This path is distinct from normal focus/activation and therefore safe
         from Alt+Tab false positives. */
      if((wp&0xFFF0)==SC_HOTKEY){
        if(!menu_tracking && !runWnd){menu_open_pending=1;menu_open_delay=1;}
        return 0;
      }
      break;
    case WM_TIMER:
      if(wp!=RAISE_TIMER)return 0;
      if(menu_tracking)return 0;
      if(menu_open_pending && !runWnd){
        if(GetKeyState(VK_CONTROL)<0 || GetKeyState(VK_MENU)<0 || GetKeyState(VK_OEM_5)<0)menu_open_delay=3;
        else if(menu_open_delay>0)menu_open_delay--;
        else {
          menu_open_pending=0;
          BringWindowToTop(h);SetActiveWindow(h);if(gButton)SetFocus(gButton);
          PostMessage(h,WM_USER+22,0,0L);
          return 0;
        }
      }
      if(GetTopWindow(NULL)!=h){
        SetWindowPos(h,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        InvalidateRect(h,NULL,FALSE);UpdateWindow(h);
      }
      return 0;
    case WM_USER+22:
      if(!menu_tracking && !runWnd)show_launch_menu();
      return 0;
    case WM_MOUSEACTIVATE:
      /* Clicking the desktop button must not itself activate the resident
         task; the button command opens the menu directly. */
      return MA_NOACTIVATE;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;BeginPaint(h,&ps);EndPaint(h,&ps);return 0;}
    case WM_SIZE:
      if(gButton)MoveWindow(gButton,0,0,LOWORD(lp),HIWORD(lp),TRUE);
      return 0;
    case WM_BEGIN_BUTTON_DRAG:{
      POINT pt;RECT wr;GetCursorPos(&pt);GetWindowRect(h,&wr);
      dragging=1;drag_grab_x=pt.x-wr.left;SetCapture(h);return 0;}
    case WM_MOUSEMOVE:
      if(dragging){
        POINT pt;RECT wr;int sw,sh,bw,bh,nx,ny;
        GetCursorPos(&pt);GetWindowRect(h,&wr);
        bw=wr.right-wr.left;bh=wr.bottom-wr.top;
        sw=GetSystemMetrics(SM_CXSCREEN);sh=GetSystemMetrics(SM_CYSCREEN);
        nx=pt.x-drag_grab_x;if(nx<0)nx=0;if(nx>sw-bw)nx=sw-bw;
        button_edge=pt.y>=sh/2?1:0;ny=button_edge?sh-bh:0;
        button_x=nx;
        SetWindowPos(h,HWND_TOP,nx,ny,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
        return 0;
      }
      break;
    case WM_LBUTTONUP:
      if(dragging){dragging=0;ReleaseCapture();save_button_position();return 0;}
      break;
    case WM_RBUTTONUP:
      if(!menu_tracking && !runWnd)show_management_menu();
      return 0;
    case WM_MANAGE_ACTION:
      if(wp==IDM_MGMT_ADD_LAUNCHER){if(load_launch_menu())add_menu_item(0);return 0;}
      if(wp==IDM_MGMT_ADD_FOLDER){if(load_launch_menu())add_menu_item(1);return 0;}
      if(wp==IDM_MGMT_ADD_SEPARATOR){if(load_launch_menu())add_menu_separator();return 0;}
      if(wp==IDM_MGMT_EDIT){begin_manage_mode(MANAGE_EDIT);return 0;}
      if(wp==IDM_MGMT_REMOVE){begin_manage_mode(MANAGE_REMOVE);return 0;}
      if(wp==IDM_MGMT_REORDER){show_reorder_dialog();return 0;}
      return 0;
    case WM_MANAGE_SELECTED:
      if((int)lp==MANAGE_EDIT)edit_menu_node((int)wp);
      else if((int)lp==MANAGE_REMOVE)remove_menu_node((int)wp);
      return 0;
    case WM_COMMAND:
      if(wp==BTN_ID){if(!menu_tracking&&!runWnd)show_launch_menu();return 0;}
      if(wp>=IDM_MGMT_ADD_LAUNCHER && wp<=IDM_MGMT_REORDER){PostMessage(h,WM_MANAGE_ACTION,wp,0L);return 0;}
      if(wp==IDM_RUN){show_run_dialog();return 0;}
      if(wp==IDM_EXPLORE){WinExec("WINFILE.EXE",SW_SHOWNORMAL);return 0;}
      if(wp==IDM_EXITWIN){if(show_exit_windows_dialog())ExitWindows(0,0);return 0;}
      if(wp>=IDM_EDIT_FIRST&&wp<IDM_EDIT_FIRST+MAX_NODES){edit_menu_node((int)wp-IDM_EDIT_FIRST);return 0;}
      if(wp>=IDM_REMOVE_FIRST&&wp<IDM_REMOVE_FIRST+MAX_NODES){remove_menu_node((int)wp-IDM_REMOVE_FIRST);return 0;}
      if(wp>=IDM_FIRST&&wp<IDM_FIRST+MAX_NODES){run_item((int)wp-IDM_FIRST);return 0;}
      break;
    case WM_DESTROY:KillTimer(h,RAISE_TIMER);gButton=NULL;PostQuitMessage(0);return 0;
  }
  return DefWindowProc(h,msg,wp,lp);
}

int PASCAL WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show)
{
  WNDCLASS wc;MSG msg;HDC dc;HWND prior;(void)show;gInst=inst;
  if(!get_base_dir())return 1;set_menu_path_from_command_line(cmd);
  /* Single resident instance.  A helper copy posts a request then exits;
     the resident deliberately waits several timer ticks before opening the
     popup so task termination/Program Manager activation is already over. */
  prior=FindWindow("LaunchWin16Button",NULL);
  if(prior){
    if(menu_path_explicit){
      ATOM a=GlobalAddAtom(menu_path);
      if(a){if(!PostMessage(prior,WM_SHOW_LAUNCH_MENU,(WPARAM)a,0L))GlobalDeleteAtom(a);}
      else PostMessage(prior,WM_SHOW_LAUNCH_MENU,0,0L);
    } else PostMessage(prior,WM_SHOW_LAUNCH_MENU,0,0L);
    BringWindowToTop(prior);SetActiveWindow(prior);
    return 0;
  }
  gWindowBrush=CreateSolidBrush(GetSysColor(COLOR_WINDOW));load_win_preferences();
  dc=GetDC(NULL);
  gDialogFont=CreateFont(-MulDiv(8,GetDeviceCaps(dc,LOGPIXELSY),72),0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");
  gButtonFont=CreateFont(-MulDiv(8,GetDeviceCaps(dc,LOGPIXELSY),72),0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH|FF_SWISS,"MS Sans Serif");
  ReleaseDC(NULL,dc);
  if(!prev){memset(&wc,0,sizeof(wc));wc.lpfnWndProc=WndProc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=NULL;wc.lpszClassName="LaunchWin16Button";
    if(!RegisterClass(&wc)){MessageBox(NULL,"Could not register Launch! Win16 window class.","Launch!",MB_OK|MB_ICONSTOP);return 1;}
    memset(&wc,0,sizeof(wc));wc.lpfnWndProc=ReorderProc;wc.hInstance=inst;wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName="LaunchReorderDialog";
    if(!RegisterClass(&wc)){MessageBox(NULL,"Could not register re-order dialog class.","Launch!",MB_OK|MB_ICONSTOP);return 1;}}
  /* Use the native Windows BUTTON again.  The popup host has no background
     brush and never erases its client area, so the rounded BUTTON corner
     pixels expose the pixels already beneath the popup instead of a fixed
     host colour.  This keeps the control visually transparent on the desktop
     while restoring standard Windows button drawing and interaction. */
  { int bw,bh,sw,sh,by; button_size(&bw,&bh);
    sw=GetSystemMetrics(SM_CXSCREEN);sh=GetSystemMetrics(SM_CYSCREEN);
    if(button_x<0)button_x=0;if(button_x>sw-bw)button_x=sw-bw;
    by=button_edge?sh-bh:0;
    gWnd=CreateWindow("LaunchWin16Button","",WS_POPUP|WS_CLIPCHILDREN,button_x,by,bw,bh,NULL,NULL,inst,NULL);
  }
  if(!gWnd){MessageBox(NULL,"Could not create Launch! Win16 button.","Launch!",MB_OK|MB_ICONSTOP);return 1;}
  ShowWindow(gWnd,SW_SHOWNOACTIVATE);
  SetWindowPos(gWnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
  UpdateWindow(gWnd);
  if(ensure_menu_file()){menu_open_pending=1;menu_open_delay=4;}
  while(GetMessage(&msg,NULL,0,0)){TranslateMessage(&msg);DispatchMessage(&msg);}
  if(dde_reply)GlobalFree(dde_reply);
  if(gButtonFont)DeleteObject(gButtonFont);
  if(gDialogFont)DeleteObject(gDialogFont);
  if(gWindowBrush)DeleteObject(gWindowBrush);
  return (int)msg.wParam;
}
