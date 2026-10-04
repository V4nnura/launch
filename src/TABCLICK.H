/* Two presses of the same tab within approximately half a second. */
static int tab_click_id=-1;
static unsigned long tab_click_tick=0;
static int tab_double_click(int id)
{
 unsigned long now=acc_ticks();
 if(id==tab_click_id && now-tab_click_tick<=9UL){tab_click_id=-1;return 1;}
 tab_click_id=id;tab_click_tick=now;return 0;
}
