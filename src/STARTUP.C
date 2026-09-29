/* Launch! 3.75 - !STARTUP Startup Editor. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include "ACCLIB.H"

#define MAX_FILES 3
#define MAX_LINES 180
#define LINE_LEN 78

typedef struct {
  char label[13], path[80];
  char line[MAX_LINES][LINE_LEN+1];
  int count, dirty, top, row;
} START_FILE;

static START_FILE sf[MAX_FILES];
static int files=0, page=0, focus=0;

static int file_exists(const char *p){FILE *f=fopen(p,"r");if(f){fclose(f);return 1;}return 0;}
static void boot_root(char *root){char *c=getenv("COMSPEC");if(c&&isalpha((unsigned char)c[0])&&c[1]==':'){root[0]=c[0];root[1]=':';root[2]='\\';root[3]=0;}else strcpy(root,"C:\\");}
static int is_4dos(void){char *v=getenv("_4VER"),*c=getenv("COMSPEC");if(v&&*v)return 1;if(c&&(strstr(c,"4DOS")||strstr(c,"4dos")))return 1;return 0;}
static void add_file(const char *label,const char *path){START_FILE *f;if(files>=MAX_FILES)return;f=&sf[files++];memset(f,0,sizeof(*f));strncpy(f->label,label,12);strncpy(f->path,path,79);}
static void load_file(START_FILE *f){FILE *in;char b[160];f->count=0;in=fopen(f->path,"r");if(!in){f->count=1;f->line[0][0]=0;return;}while(f->count<MAX_LINES&&fgets(b,sizeof(b),in)){b[strcspn(b,"\r\n")]=0;strncpy(f->line[f->count],b,LINE_LEN);f->line[f->count][LINE_LEN]=0;f->count++;}fclose(in);if(!f->count)f->count=1;}
static int confirm_save(const char *name){int k,sel=1;for(;;){acc_subbox(22,8,36,8,"Save startup file",0);acc_text(25,10,"Save changes to",ACC_LABEL,16);acc_text(25,11,name,ACC_HEADING,16);acc_button(30,13,"Save",sel==0);acc_button(43,13,"Cancel",sel==1);k=acc_key();if(k==27)return 0;if(k==9||k==0x4b00||k==0x4d00){sel=!sel;continue;}if(k==13)return sel==0;if(k=='s'||k=='S')return 1;}}
static void save_file(START_FILE *f){FILE *o;int i;if(!confirm_save(f->label))return;o=fopen(f->path,"w");if(!o){acc_notice("Startup Editor","Unable to save startup file.");return;}for(i=0;i<f->count;i++)fprintf(o,"%s\n",f->line[i]);fclose(o);f->dirty=0;acc_notice("Startup Editor","Startup file saved.");}
static int typo(const char *s,const char *bad,const char *good,char *msg,int n){if(strstr(s,bad)){sprintf(msg,"Possible '%s' typo; did you mean %s?",bad,good);return 1;}return 0;}
static void check_file(START_FILE *f){int i;char u[LINE_LEN+1],msg[120];for(i=0;i<f->count;i++){int j;strncpy(u,f->line[i],LINE_LEN);u[LINE_LEN]=0;for(j=0;u[j];j++)u[j]=(char)toupper((unsigned char)u[j]);if(typo(u,"LOADHGH","LOADHIGH",msg,sizeof(msg))||typo(u,"DEIVCE=","DEVICE=",msg,sizeof(msg))||typo(u,"DEVIVE=","DEVICE=",msg,sizeof(msg))||typo(u,"SET PATH =","SET PATH=",msg,sizeof(msg))||typo(u,"LHIGH ","LOADHIGH ",msg,sizeof(msg))){char out[150];sprintf(out,"Line %d: %s",i+1,msg);acc_notice("Check startup file",out);return;}if(strchr(u,'=')==0&&(strstr(u,"DEVICEHIGH ")||strstr(u,"DEVICE "))){sprintf(msg,"Line %d: DEVICE/DEVICEHIGH normally requires '='.",i+1);acc_notice("Check startup file",msg);return;}if(strlen(f->line[i])>=LINE_LEN){sprintf(msg,"Line %d is unusually long and may be truncated.",i+1);acc_notice("Check startup file",msg);return;}}
 acc_notice("Check startup file","No obvious syntax problems found.");}
static void draw(void){START_FILE *f=&sf[page];int i,y;char n[8];acc_clear(ACC_BG);acc_box(1,0,78,25,"Startup Editor");acc_text(4,2,"Startup Files",ACC_HEADING,18);for(i=0;i<files;i++){int x=4+i*17;acc_text(x,4,sf[i].label,i==page?ACC_SELECT:ACC_CONTROL,13);}acc_text(4,6,"Line",ACC_LABEL,4);acc_text(10,6,f->path,ACC_LABEL,62);for(i=0;i<14;i++){int li=f->top+i;y=7+i;if(li<f->count){sprintf(n,"%3d",li+1);acc_text(4,y,n,ACC_LABEL,3);acc_text(9,y,f->line[li],(focus==0&&li==f->row)?ACC_SELECT:ACC_TEXT,66);}else acc_text(4,y,"",ACC_TEXT,71);}acc_button(4,22,"Check",focus==1);acc_button(16,22,"Save",focus==2);acc_button(67,22,"Close",focus==3);}
static void edit_line(START_FILE *f){char *s=f->line[f->row];int k,n=(int)strlen(s);for(;;){draw();acc_caret_set(9+n,7+f->row-f->top);k=acc_key();acc_caret_hide();if(k==27||k==13)break;if(k==8&&n){s[--n]=0;f->dirty=1;}else if(k>=32&&k<127&&n<LINE_LEN){s[n++]=(char)k;s[n]=0;f->dirty=1;}}}
int main(int argc,char **argv){char root[8],p[80];int k;if(acc_help(argc,argv,"!STARTUP","Edit and check DOS startup files."))return 0;boot_root(root);sprintf(p,"%sAUTOEXEC.BAT",root);if(!file_exists(p)){char q[80];sprintf(q,"%sFDAUTO.BAT",root);if(file_exists(q))strcpy(p,q);}add_file(strstr(p,"FDAUTO")?"FDAUTO.BAT":"AUTOEXEC.BAT",p);sprintf(p,"%sCONFIG.SYS",root);add_file("CONFIG.SYS",p);if(is_4dos()){sprintf(p,"%s4START.BAT",root);add_file("4START.BAT",p);}for(k=0;k<files;k++)load_file(&sf[k]);if(!acc_begin(argv[0],"Startup Editor",0))return 1;for(;;){START_FILE *f=&sf[page];draw();k=acc_key();if(k==27)break;if(k==9){focus=(focus+1)%4;continue;}if(k==0x0f00){focus=(focus+3)%4;continue;}if(k==0x4b00&&focus==0&&page>0){page--;continue;}if(k==0x4d00&&focus==0&&page+1<files){page++;continue;}if(k==0x4800&&focus==0){if(f->row>0)f->row--;if(f->row<f->top)f->top=f->row;continue;}if(k==0x5000&&focus==0){if(f->row+1<f->count)f->row++;else if(f->count<MAX_LINES){f->row=f->count++;f->line[f->row][0]=0;}if(f->row>=f->top+14)f->top=f->row-13;continue;}if(k==13){if(focus==0)edit_line(f);else if(focus==1)check_file(f);else if(focus==2)save_file(f);else break;}}
acc_end();return 0;}
