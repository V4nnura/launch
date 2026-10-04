/* Windows 3.0-compatible regional clock (WIN.INI [intl]). */
#include <time.h>
static int clock_mode=0,clock_24=0,clock_zero=0;
static char clock_sep[9],clock_am[25],clock_pm[25],clock_text[64];
static void clock_locale(void)
{
  clock_24=GetProfileInt("intl","iTime",0);
  clock_zero=GetProfileInt("intl","iTLZero",0);
  GetProfileString("intl","sTime",":",clock_sep,sizeof(clock_sep));
  GetProfileString("intl","s1159","AM",clock_am,sizeof(clock_am));
  GetProfileString("intl","s2359","PM",clock_pm,sizeof(clock_pm));
}
static void clock_format(char *out,int hour,int minute,int second)
{
  const char *suffix=clock_24?clock_pm:(hour<12?clock_am:clock_pm);
  if(!clock_24){hour%=12;if(!hour)hour=12;}
  sprintf(out,clock_zero?"%02d%s%02d%s%02d%s%s":"%d%s%02d%s%02d%s%s",
          hour,clock_sep,minute,clock_sep,second,*suffix?" ":"",suffix);
}
static int clock_update(void)
{
  time_t now;struct tm *t;char next[64];
  if(!clock_mode)return 0;
  time(&now);t=localtime(&now);if(!t)return 0;
  clock_format(next,t->tm_hour,t->tm_min,t->tm_sec);
  if(!strcmp(next,clock_text))return 0;
  strcpy(clock_text,next);return 1;
}
