/* Launch! 3.8 !MDVIEW compatibility alias.
   The Markdown viewer itself lives only in !MD.EXE /VIEW. */
#include <stdio.h>
#include <string.h>
#include <process.h>
#define AP 128
#define MAXARGS 66

static void sibling(const char *argv0,const char *name,char *out)
{
 const char *slash,*other;int n;
 slash=strrchr(argv0,'\\');other=strrchr(argv0,'/');if(!slash||(other&&other>slash))slash=other;
 if(slash){n=(int)(slash-argv0)+1;if(n>AP-(int)strlen(name)-1)n=0;if(n){strncpy(out,argv0,n);out[n]=0;strcat(out,name);return;}}
 if(argv0[0]&&argv0[1]==':'){out[0]=argv0[0];out[1]=':';out[2]=0;strcat(out,name);return;}
 strcpy(out,name);
}

int main(int argc,char **argv)
{
 char target[AP];char *av[MAXARGS];int i,rc;
 if(argc+2>MAXARGS){puts("!MDVIEW: command line has too many arguments.");return 1;}
 sibling(argv[0],"!MD.EXE",target);
 av[0]=target;av[1]="/VIEW";for(i=1;i<argc;i++)av[i+1]=argv[i];av[argc+1]=0;
 rc=spawnv(P_OVERLAY,target,(const char * const *)av);
 /* If invoked by a bare name and !MD is not in the current directory, allow
    the normal DOS PATH lookup as a compatibility fallback. */
 av[0]="!MD.EXE";rc=spawnvp(P_OVERLAY,"!MD.EXE",(const char * const *)av);
 puts("!MDVIEW requires !MD.EXE");return rc<0?1:rc;
}
