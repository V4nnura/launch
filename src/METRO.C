/*
    __                           __    __
   / /   ____ ___  ______  _____/ /_  / /
  / /   / __ `/ / / / __ \/ ___/ __ \/ /
 / /___/ /_/ / /_/ / / / / /__/ / / /_/
/_____/\__,_/\__,_/_/ /_/\___/_/ /_(_)
Launch! for DOS ---------------------
*/
/*
 * Launch! !METRO prototype - Release 3.74
 *
 * Text-mode transport-network game inspired by schematic metro maps.
 *
 * The world is larger than the visible viewport.  With no route under
 * construction the arrow keys pan the map.  Click or Tab to select a station
 * and use 1/2/3 to choose a line before drawing a route; arrow keys then lay one logical track cell at a time.
 * Reaching a cell adjacent to another station connects that station
 * automatically.  Tracks never depend on mouse-motion interpolation.
 * Crowding warnings use a three-beep alert; a failed station is centered and
 * the map colour-cycles before the game-over notice.  Relief gifts can add
 * extra, evenly spaced trains to a line when a heavily loaded station is
 * already served by all available lines.  Levels 3-5 add a one-cell-wide
 * river which blocks route construction unless a one-use bridge gift is
 * available.
 *
 * Metro-specific VGA glyphs are installed only while the game is running and
 * restored on exit.  The artwork was supplied in LAUNCHUI.FNT at the source
 * character positions documented below; track artwork is relocated at runtime
 * because those source positions overlap Launch!/!PLUMB UI glyph positions.
 *
 * EGA currently uses ASCII/CP437 fallbacks until the matching 8x14 artwork is
 * supplied.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <time.h>
#include "ACCLIB.H"

#define WORLD_W 60
#define WORLD_H 40
#define VIEW_W 27
#define VIEW_H 14
#define VIEW_CELLS_W (VIEW_W * 2)

#define MAX_STATIONS 25
#define MAX_LINES 3
#define MAX_ROUTE 18
#define MAX_PATH 600
#define TRAIN_CAP 8
#define MAX_TRAINS 4

#define ST_CIRCLE 0
#define ST_TRIANGLE 1
#define ST_SQUARE 2

#define BROWN_BG 6
#define GRASS_BG 2
#define WATER_BG 1

#define WALL_L 193
#define WALL_R 195

#define MG_PERSON 133
#define MG_TRAIN 134
#define MG_TRI_L 135
#define MG_TRI_R 136
#define MG_CIR_L 137
#define MG_CIR_R 138
#define MG_SQR_L 139
#define MG_SQR_R 140
#define MG_NW_L 194
#define MG_NW_R 197
#define MG_SW_L 199
#define MG_SW_R 202
#define MG_X_L 203
#define MG_X_R 204
#define MG_H 205
#define MG_SE_L 206
#define MG_SE_R 207
#define MG_NE_L 208
#define MG_NE_R 209
#define MG_V_L 210
#define MG_V_R 212

#define GIDX_MG_PERSON 0
#define GIDX_MG_TRAIN 1
#define GIDX_MG_TRI_L 2
#define GIDX_MG_TRI_R 3
#define GIDX_MG_CIR_L 4
#define GIDX_MG_CIR_R 5
#define GIDX_MG_SQR_L 6
#define GIDX_MG_SQR_R 7
#define GIDX_MG_NW_L 8
#define GIDX_MG_NW_R 9
#define GIDX_MG_SW_L 10
#define GIDX_MG_SW_R 11
#define GIDX_MG_X_L 12
#define GIDX_MG_X_R 13
#define GIDX_MG_H 14
#define GIDX_MG_SE_L 15
#define GIDX_MG_SE_R 16
#define GIDX_MG_NE_L 17
#define GIDX_MG_NE_R 18
#define GIDX_MG_V_L 19
#define GIDX_MG_V_R 20

typedef struct {
  int x, y, type;
  int wait[3];
  int crowd_ticks;
  int warning_sent;
} METRO_STATION;

typedef struct {
  int station[MAX_ROUTE];
  int stop_path[MAX_ROUTE];
  int count;
  int colour;
  unsigned char path_x[MAX_PATH];
  unsigned char path_y[MAX_PATH];
  int path_count;
  int train_count;
  int train_phase[MAX_TRAINS];
  int pax[MAX_TRAINS][3];
} METRO_LINE;

static METRO_STATION stations[MAX_STATIONS];
static METRO_LINE lines[MAX_LINES];
static unsigned char terrain[WORLD_H][WORLD_W];
static int water_vertical;
static int water_pos[WORLD_W > WORLD_H ? WORLD_W : WORLD_H];
static int station_count;
static int active_line;
static int line_armed;
static int score;
static int paused;
static int game_over;
static int level = 1;
static int pan_x, pan_y;
static int draw_active;
static int draw_start_path;
static int draw_bridge_used;
static int bridge_crossing_active;
static int route_cursor_x, route_cursor_y;
static int selected_station = -1;
static int selected_destination = -1;
static unsigned long last_tick;
static unsigned long sim_ticks;
static unsigned long last_station_spawn;
static unsigned long last_passenger_spawn;
static int warning_station = -1;
static int station_visible(int s);
static int failed_station = -1;
static int failure_shown;
static int gift_ready;
static int gift_latched;
static int bridge_ready;
static int pending_train_gift;
static int board_flash_bg = -1;

static int line_colours[MAX_LINES] = {13, 14, 12}; /* bright magenta, yellow, red */

/*
 * VGA track artwork lives in the shared Launch! font at the same source
 * character positions used by !PLUMB.  !METRO copies those glyphs into a
 * private set of VGA line-graphics slots at startup.  The targets deliberately
 * avoid Launch! UI glyphs used by the Metro titlebar, dialog OK button, button
 * shadows and outer frame; changing those slots corrupts UI while Metro runs.  Station/person/train glyphs
 * already occupy their final Metro positions (133-140).
 */
static const unsigned char metro_track_source[13] = {
  195,196, 211,212, 213,214, 215, 217,218, 201,202, 197,198
};
static const unsigned char metro_track_target[13] = {
  MG_NW_L,MG_NW_R,MG_SW_L,MG_SW_R,MG_X_L,MG_X_R,MG_H,
  MG_SE_L,MG_SE_R,MG_NE_L,MG_NE_R,MG_V_L,MG_V_R
};
/* Embedded copies of the Metro VGA artwork.  Do not assume the user's
 * currently selected display font contains these application-specific
 * glyphs.  Install them temporarily while !METRO is active, then restore
 * the caller's font on exit.  Entries 0-7 are person/train/stations;
 * entries 8-20 are the track pieces in the source order below. */
static const unsigned char metro_vga_glyphs[21][32] = {
  {0x00,0x3C,0x7E,0x7E,0x3C,0x18,0x7E,0xFF,0xDB,0x18,0x3C,0x3C,0x66,0x66,0xC3,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x7E,0x81,0x81,0x81,0xFF,0xFF,0xBD,0xFF,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x01,0x02,0x02,0x04,0x08,0x00,0x10,0x20,0x20,0x40,0x80,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x80,0x40,0x40,0x20,0x10,0x00,0x08,0x04,0x04,0x02,0x01,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x03,0x0C,0x10,0x20,0x20,0x40,0x00,0x40,0x20,0x20,0x10,0x0C,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0xC0,0x30,0x08,0x04,0x04,0x02,0x00,0x02,0x04,0x04,0x08,0x30,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x7F,0x40,0x40,0x40,0x40,0x40,0x00,0x40,0x40,0x40,0x40,0x40,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0xFE,0x02,0x02,0x02,0x02,0x02,0x00,0x02,0x02,0x02,0x02,0x02,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0x03,0x05,0x06,0x07,0x07,0x07,0x04,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xFF,0xED,0xED,0x6D,0xBF,0xF0,0x20,0xE0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x07,0x07,0x06,0x05,0x07,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xF0,0x3F,0xED,0xED,0xED,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x0F,0xFC,0x6F,0x6D,0x6D,0xFC,0x0F,0x04,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xF0,0x3F,0xAD,0xAD,0xED,0x3F,0xF0,0x20,0xE0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xFF,0x6D,0x6D,0x6D,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x0F,0xFD,0x6E,0x6F,0x6F,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xE0,0xE0,0xE0,0x20,0xE0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xFF,0x6F,0x6E,0x6D,0xFB,0x0F,0x04,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xC0,0x60,0xE0,0xE0,0xE0,0xE0,0x20,0xE0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};
static const unsigned char metro_ega_glyphs[21][32] = {
  {0x3C,0x7E,0x7E,0x3C,0x18,0x7E,0xFF,0xDB,0x18,0x3E,0x63,0x63,0x63,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x7E,0x81,0x81,0x81,0xFF,0xFF,0xBD,0xFF,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x01,0x02,0x02,0x04,0x08,0x00,0x10,0x20,0x20,0x40,0x80,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x80,0x40,0x40,0x20,0x10,0x00,0x08,0x04,0x04,0x02,0x01,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x06,0x18,0x20,0x40,0x40,0x80,0x00,0x80,0x40,0x40,0x20,0x18,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0xC0,0x30,0x08,0x04,0x04,0x02,0x00,0x02,0x04,0x04,0x08,0x30,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x7F,0x40,0x40,0x40,0x40,0x40,0x00,0x40,0x40,0x40,0x40,0x40,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0xFE,0x02,0x02,0x02,0x02,0x02,0x00,0x02,0x02,0x02,0x02,0x02,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0x03,0x07,0x07,0x07,0x04,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xFF,0xED,0xED,0xED,0x3F,0xF0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x07,0x07,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xF0,0x3F,0xED,0xED,0xED,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x0F,0xFC,0x6F,0x6C,0x6F,0xFC,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xF0,0x3F,0xED,0x2D,0xED,0x3F,0xF0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xFF,0x6D,0x6D,0x6D,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x0F,0xFC,0x6F,0x6F,0x6F,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0xE0,0xE0,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xFF,0x6F,0x6F,0x6F,0xFC,0x0F,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x00,0x00,0x00,0xC0,0xE0,0xE0,0xE0,0x20,0xE0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x07,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
  {0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0xE0,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};
static unsigned char metro_old_glyph[21][32];
static unsigned char metro_old_wall[2][32];
static int metro_font_active;

static int crowd_limit(void)
{
  /* Early maps are deliberately forgiving; pressure rises gradually. */
  int v = 12 - (level - 1) / 4;
  return v < 6 ? 6 : v;
}

static unsigned long passenger_delay(void)
{
  /* Level 1 is a teaching level.  By level 25 demand is substantially faster. */
  long v = 105L - (long)(level - 1) * 3L;
  return (unsigned long)(v < 30L ? 30L : v);
}

static unsigned long station_delay(void)
{
  long v = 1100L - (long)(level - 1) * 32L;
  return (unsigned long)(v < 300L ? 300L : v);
}

static int station_cap(void)
{
  int v = 3 + (level - 1) / 2;
  return v > 15 ? 15 : v;
}

static int water_enabled(void)
{
  return level >= 9;
}

static void metro_font(int install)
{
  int i;

  if (install) {
    if (metro_font_active)
      return;

    /* Person/train/station artwork is installed at its public Metro slots.
       Track artwork is installed into private slots 141-153.  Saving every
       replaced slot keeps !METRO independent of the user's selected VGA font. */
    for (i = 0; i < 8; ++i) {
      acc_glyph_read(MG_PERSON + i, metro_old_glyph[i]);
      acc_glyph_write(MG_PERSON + i, acc_font_height()==14?metro_ega_glyphs[i]:metro_vga_glyphs[i]);
    }
    for (i = 0; i < 13; ++i) {
      acc_glyph_read(metro_track_target[i], metro_old_glyph[8 + i]);
      acc_glyph_write(metro_track_target[i], acc_font_height()==14?metro_ega_glyphs[8+i]:metro_vga_glyphs[8+i]);
    }

    acc_glyph_read(WALL_L, metro_old_wall[0]);
    acc_glyph_read(WALL_R, metro_old_wall[1]);
    acc_glyph_library(42, WALL_L);
    acc_glyph_library(43, WALL_R);
    metro_font_active = 1;
  } else if (metro_font_active) {
    for (i = 0; i < 8; ++i)
      acc_glyph_write(MG_PERSON + i, metro_old_glyph[i]);
    for (i = 0; i < 13; ++i)
      acc_glyph_write(metro_track_target[i], metro_old_glyph[8 + i]);
    acc_glyph_write(WALL_L, metro_old_wall[0]);
    acc_glyph_write(WALL_R, metro_old_wall[1]);
    metro_font_active = 0;
  }
}

static int total_wait(int s)
{
  return stations[s].wait[0] + stations[s].wait[1] + stations[s].wait[2];
}

static int total_train_pax(METRO_LINE *ln, int ti)
{
  return ln->pax[ti][0] + ln->pax[ti][1] + ln->pax[ti][2];
}

static int line_has_type(METRO_LINE *ln, int type)
{
  int i;
  for (i = 0; i < ln->count; ++i)
    if (stations[ln->station[i]].type == type)
      return 1;
  return 0;
}

static int route_contains(METRO_LINE *ln, int station)
{
  int i;
  for (i = 0; i < ln->count; ++i)
    if (ln->station[i] == station)
      return 1;
  return 0;
}

/*
 * Small timing helper used by PC-speaker and failure animations.
 * Borland provides delay(), but Microsoft C 7 does not.  Use the C runtime
 * clock instead so !METRO does not acquire a compiler-specific dependency.
 */
static void metro_delay(unsigned ms)
{
  clock_t start = clock();
  clock_t ticks = (clock_t)(((unsigned long)ms * (unsigned long)CLOCKS_PER_SEC + 999UL) / 1000UL);
  if (ticks < 1)
    ticks = 1;
  while ((clock_t)(clock() - start) < ticks)
    ;
}

static int station_near(int x, int y)
{
  int i, dx, dy;
  for (i = 0; i < station_count; ++i) {
    dx = stations[i].x - x;
    if (dx < 0)
      dx = -dx;
    dy = stations[i].y - y;
    if (dy < 0)
      dy = -dy;
    if (dx < 5 && dy < 3)
      return 1;
  }
  return 0;
}

static void metro_pop(void)
{
  unsigned divisor = (unsigned)(1193180UL / 180UL);
  unsigned char old = (unsigned char)inp(0x61);
  outp(0x43, 0xB6);
  outp(0x42, divisor & 0xFF);
  outp(0x42, divisor >> 8);
  outp(0x61, old | 3);
  metro_delay(18);
  outp(0x61, old & 0xFC);
}

static int add_station_at(int x, int y, int type, int sound_it)
{
  METRO_STATION *s;
  if (station_count >= MAX_STATIONS)
    return -1;
  s = &stations[station_count];
  memset(s, 0, sizeof(*s));
  s->x = x;
  s->y = y;
  s->type = type;
  if (sound_it)
    metro_pop();
  return station_count++;
}

static int station_water_side_xy(int x, int y)
{
  int p;
  if (!water_enabled())
    return 0;
  if (water_vertical) {
    p = water_pos[y];
    return (x < p) ? -1 : 1;
  }
  p = water_pos[x];
  return (y < p) ? -1 : 1;
}

static void maybe_offer_bridge_for_station(int new_station)
{
  int i, side;
  if (!water_enabled() || new_station <= 0 || bridge_ready)
    return;
  side = station_water_side_xy(stations[new_station].x, stations[new_station].y);
  for (i = 0; i < new_station; ++i) {
    if (station_water_side_xy(stations[i].x, stations[i].y) != side) {
      bridge_ready = 1;
      return;
    }
  }
}

static void add_random_station(void)
{
  int tries, x, y, s;
  if (station_count >= MAX_STATIONS || station_count >= station_cap())
    return;
  for (tries = 0; tries < 100; ++tries) {
    x = 2 + rand() % (WORLD_W - 4);
    y = 2 + rand() % (WORLD_H - 4);
    if (terrain[y][x] != 2 && !station_near(x, y)) {
      s = add_station_at(x, y, rand() % 3, 1);
      maybe_offer_bridge_for_station(s);
      return;
    }
  }
}

static void generate_terrain(void)
{
  int i, j, cx, cy, dx, dy, p, run;
  memset(terrain, 0, sizeof(terrain));
  memset(water_pos, 0, sizeof(water_pos));
  for (i = 0; i < 20; ++i) {
    cx = rand() % WORLD_W;
    cy = rand() % WORLD_H;
    for (j = 0; j < 10 + rand() % 18; ++j) {
      dx = cx + (rand() % 7) - 3;
      dy = cy + (rand() % 5) - 2;
      if (dx >= 0 && dx < WORLD_W && dy >= 0 && dy < WORLD_H)
        terrain[dy][dx] = 1;
    }
  }

  if (!water_enabled())
    return;

  water_vertical = rand() & 1;
  run = 0;
  if (water_vertical) {
    int oldp;
    p = WORLD_W / 2 + (rand() % 9) - 4;
    for (i = 0; i < WORLD_H; ++i) {
      oldp = p;
      if (--run <= 0) {
        p += (rand() % 3) - 1;
        if (p < 7) p = 7;
        if (p > WORLD_W - 8) p = WORLD_W - 8;
        run = 3 + rand() % 5;
      }
      water_pos[i] = p;
      terrain[i][p] = 2;
      if (oldp != p)
        terrain[i][oldp] = 2;
    }
  } else {
    int oldp;
    p = WORLD_H / 2 + (rand() % 7) - 3;
    for (i = 0; i < WORLD_W; ++i) {
      oldp = p;
      if (--run <= 0) {
        p += (rand() % 3) - 1;
        if (p < 5) p = 5;
        if (p > WORLD_H - 6) p = WORLD_H - 6;
        run = 4 + rand() % 6;
      }
      water_pos[i] = p;
      terrain[p][i] = 2;
      if (oldp != p)
        terrain[oldp][i] = 2;
    }
  }
}

static int add_initial_station(int x, int y, int type)
{
  int nx = x, ny = y;
  if (water_enabled() && terrain[ny][nx] == 2) {
    if (water_vertical) {
      nx += (nx < WORLD_W / 2) ? -2 : 2;
      if (nx < 2) nx = 2;
      if (nx > WORLD_W - 3) nx = WORLD_W - 3;
    } else {
      ny += (ny < WORLD_H / 2) ? -2 : 2;
      if (ny < 2) ny = 2;
      if (ny > WORLD_H - 3) ny = WORLD_H - 3;
    }
  }
  return add_station_at(nx, ny, type, 0);
}

static void reset_game(void)
{
  int i;
  memset(stations, 0, sizeof(stations));
  memset(lines, 0, sizeof(lines));
  station_count = 0;
  score = 0;
  active_line = 0;
  line_armed = 0;
  paused = 0;
  game_over = 0;
  warning_station = -1;
  failed_station = -1;
  failure_shown = 0;
  gift_ready = 0;
  gift_latched = 0;
  bridge_ready = 0;
  pending_train_gift = 0;
  board_flash_bg = -1;
  sim_ticks = 0;
  draw_active = 0;
  draw_start_path = 0;
  draw_bridge_used = 0;
  bridge_crossing_active = 0;
  selected_station = -1;
  selected_destination = -1;
  pan_x = 0;
  pan_y = 0;
  generate_terrain();
  /* Start compact and readable; later levels spread the initial network out. */
  if (level <= 4) {
    add_initial_station(25, 18, ST_CIRCLE);
    add_initial_station(30, 18, ST_TRIANGLE);
    add_initial_station(27, 22, ST_SQUARE);
  } else if (level <= 12) {
    add_initial_station(18, 13, ST_CIRCLE);
    add_initial_station(31, 18, ST_TRIANGLE);
    add_initial_station(40, 24, ST_SQUARE);
  } else {
    add_initial_station(10, 9, ST_CIRCLE);
    add_initial_station(31, 19, ST_TRIANGLE);
    add_initial_station(50, 29, ST_SQUARE);
  }
  /* Initial view follows the first (top-left) station rather than world origin. */
  pan_x = stations[0].x - VIEW_W / 2;
  pan_y = stations[0].y - VIEW_H / 2;
  if (pan_x < 0) pan_x = 0; if (pan_y < 0) pan_y = 0;
  if (pan_x > WORLD_W - VIEW_W) pan_x = WORLD_W - VIEW_W;
  if (pan_y > WORLD_H - VIEW_H) pan_y = WORLD_H - VIEW_H;
  if (water_enabled()) {
    maybe_offer_bridge_for_station(1);
    maybe_offer_bridge_for_station(2);
  }
  for (i = 0; i < MAX_LINES; ++i) {
    lines[i].colour = line_colours[i];
    lines[i].train_count = 0;
  }
  last_tick = acc_ticks();
  last_station_spawn = 0;
  last_passenger_spawn = 0;
}

static int world_visible_x(int wx)
{
  return wx >= pan_x && wx < pan_x + VIEW_W;
}

static int world_visible_y(int wy)
{
  return wy >= pan_y && wy < pan_y + VIEW_H;
}

static int world_to_screen_x(int ox, int wx)
{
  return ox + (wx - pan_x) * 2;
}

static int world_to_screen_y(int oy, int wy)
{
  return oy + (wy - pan_y);
}

static int bg_for_world(int wx, int wy)
{
  if (board_flash_bg >= 0)
    return board_flash_bg;
  if (wx < 0 || wx >= WORLD_W || wy < 0 || wy >= WORLD_H)
    return BROWN_BG;
  if (terrain[wy][wx] == 2)
    return WATER_BG;
  return terrain[wy][wx] ? GRASS_BG : BROWN_BG;
}

static void wall_pair(int x, int y)
{
  int a = ACC_ATTR(3, 11); /* cyan brick: cyan background, bright-cyan foreground */
  acc_put(x, y, WALL_L, a);
  acc_put(x + 1, y, WALL_R, a);
}

static void alarm_wall_pair(int x, int y)
{
  int a = ACC_ATTR(4, 14); /* red background, bright-yellow warning brick */
  acc_put(x, y, WALL_L, a);
  acc_put(x + 1, y, WALL_R, a);
}

static int route_cell_used(int wx, int wy)
{
  int li, i;
  for (li = 0; li < MAX_LINES; ++li)
    for (i = 0; i < lines[li].path_count; ++i)
      if ((int)lines[li].path_x[i] == wx && (int)lines[li].path_y[i] == wy)
        return 1;
  return 0;
}

static void track_pair_for(METRO_LINE *ln, int i, int *l, int *r)
{
  int x, y, px, py, nx, ny;
  int left = 0, right = 0, up = 0, down = 0;
  x = ln->path_x[i];
  y = ln->path_y[i];
  if (i > 0) {
    px = ln->path_x[i - 1];
    py = ln->path_y[i - 1];
    if (px < x) left = 1;
    else if (px > x) right = 1;
    else if (py < y) up = 1;
    else if (py > y) down = 1;
  }
  if (i + 1 < ln->path_count) {
    nx = ln->path_x[i + 1];
    ny = ln->path_y[i + 1];
    if (nx < x) left = 1;
    else if (nx > x) right = 1;
    else if (ny < y) up = 1;
    else if (ny > y) down = 1;
  }
  if (left && right && up && down) { *l = MG_X_L; *r = MG_X_R; }
  else if (right && down) { *l = MG_NW_L; *r = MG_NW_R; }
  else if (left && down) { *l = MG_NE_L; *r = MG_NE_R; }
  else if (right && up) { *l = MG_SW_L; *r = MG_SW_R; }
  else if (left && up) { *l = MG_SE_L; *r = MG_SE_R; }
  else if (up || down) { *l = MG_V_L; *r = MG_V_R; }
  else { *l = MG_H; *r = MG_H; }
}

static void draw_route(int ox, int oy, int li)
{
  METRO_LINE *ln = &lines[li];
  int i, sx, sy, l, r, bg;
  for (i = 0; i < ln->path_count; ++i) {
    if (!world_visible_x(ln->path_x[i]) || !world_visible_y(ln->path_y[i]))
      continue;
    sx = world_to_screen_x(ox, ln->path_x[i]);
    sy = world_to_screen_y(oy, ln->path_y[i]);
    bg = (terrain[ln->path_y[i]][ln->path_x[i]] == 2) ? 15 :
         bg_for_world(ln->path_x[i], ln->path_y[i]);
    track_pair_for(ln, i, &l, &r);
    acc_put(sx, sy, l, ACC_ATTR(bg, ln->colour));
    acc_put(sx + 1, sy, r, ACC_ATTR(bg, ln->colour));
  }
}

static int station_selected_colour(int s)
{
  if (s == selected_station || s == selected_destination)
    return lines[active_line].colour;
  return -1;
}

static void station_glyph_pair(int type, int *l, int *r)
{
  if (type == ST_TRIANGLE) { *l = MG_TRI_L; *r = MG_TRI_R; }
  else if (type == ST_SQUARE) { *l = MG_SQR_L; *r = MG_SQR_R; }
  else { *l = MG_CIR_L; *r = MG_CIR_R; }
}

static void draw_station(int ox, int oy, int i)
{
  METRO_STATION *s = &stations[i];
  int fg, bg, sx, sy, l, r, q;
  char buf[8];
  if (!world_visible_x(s->x) || !world_visible_y(s->y))
    return;
  sx = world_to_screen_x(ox, s->x);
  sy = world_to_screen_y(oy, s->y);
  fg = station_selected_colour(i);
  station_glyph_pair(s->type, &l, &r);
  if (board_flash_bg >= 0) {
    bg = board_flash_bg; fg = 15;
    acc_put(sx, sy, l, ACC_ATTR(bg, fg));
    acc_put(sx + 1, sy, r, ACC_ATTR(bg, fg));
  } else if (fg < 0) {
    acc_put(sx, sy, l, ACC_CONTROL);
    acc_put(sx + 1, sy, r, ACC_CONTROL);
  } else {
    bg = fg & 7;
    acc_put(sx, sy, l, ACC_ATTR(bg, fg));
    acc_put(sx + 1, sy, r, ACC_ATTR(bg, fg));
  }
  q = total_wait(i);
  if (sy + 1 < oy + VIEW_H) {
    acc_put(sx, sy + 1, MG_PERSON,
            ACC_ATTR(bg_for_world(s->x, s->y), 15));
    sprintf(buf, "%d", q);
    acc_text(sx + 1, sy + 1, buf, ACC_ATTR(bg_for_world(s->x, s->y), 15), 3);
  }
}

static int train_path_pos(METRO_LINE *ln, int ti)
{
  int cycle, ph;
  if (ln->path_count < 2)
    return 0;
  cycle = (ln->path_count - 1) * 2;
  if (cycle <= 0)
    return 0;
  ph = ln->train_phase[ti] % cycle;
  return (ph <= ln->path_count - 1) ? ph : cycle - ph;
}

static void draw_trains(int ox, int oy, METRO_LINE *ln)
{
  int ti, pos, wx, wy, sx, sy, n, bg;
  char b[4];
  if (ln->count < 2 || ln->path_count < 2)
    return;
  for (ti = 0; ti < ln->train_count; ++ti) {
    pos = train_path_pos(ln, ti);
    wx = ln->path_x[pos];
    wy = ln->path_y[pos];
    if (!world_visible_x(wx) || !world_visible_y(wy))
      continue;
    sx = world_to_screen_x(ox, wx);
    sy = world_to_screen_y(oy, wy);
    bg = (terrain[wy][wx] == 2) ? 15 : bg_for_world(wx, wy);
    acc_put(sx, sy, MG_TRAIN,
            ACC_ATTR(bg, ln->colour));
    n = total_train_pax(ln, ti);
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    sprintf(b, "%d", n);
    if (sy > oy)
      acc_text(sx, sy - 1, b, ACC_ATTR(bg_for_world(wx, wy - 1), ln->colour), 1);
    else if (sx + 2 < ox + VIEW_CELLS_W)
      acc_text(sx + 1, sy, b, ACC_ATTR(bg, ln->colour), 1);
  }
}

static void draw_map(int ox, int oy)
{
  int i, vx, vy, wx, wy, bg;
  for (vy = 0; vy < VIEW_H; ++vy) {
    wy = pan_y + vy;
    for (vx = 0; vx < VIEW_W; ++vx) {
      wx = pan_x + vx;
      bg = bg_for_world(wx, wy);
      acc_put(ox + vx * 2, oy + vy, ' ', ACC_ATTR(bg, 0));
      acc_put(ox + vx * 2 + 1, oy + vy, ' ', ACC_ATTR(bg, 0));
    }
  }
  for (i = 0; i < MAX_LINES; ++i)
    draw_route(ox, oy, i);
  for (i = 0; i < station_count; ++i)
    draw_station(ox, oy, i);
  for (i = 0; i < MAX_LINES; ++i)
    draw_trains(ox, oy, &lines[i]);

  for (vx = -2; vx < VIEW_CELLS_W + 2; vx += 2) {
    wall_pair(ox + vx, oy - 1);
    wall_pair(ox + vx, oy + VIEW_H);
  }
  for (vy = 0; vy < VIEW_H; ++vy) {
    wall_pair(ox - 2, oy + vy);
    wall_pair(ox + VIEW_CELLS_W, oy + vy);
  }
  /* When the crowded station is off-screen, project its position onto the
     nearest playfield edge.  The red/yellow brick acts as an alarm bearing. */
  if (warning_station >= 0 && !station_visible(warning_station)) {
    int awx = stations[warning_station].x, awy = stations[warning_station].y;
    int ax = awx - pan_x, ay = awy - pan_y;
    if (ax < 0) { if (ay < 0) ay = 0; if (ay >= VIEW_H) ay = VIEW_H-1; alarm_wall_pair(ox-2, oy+ay); }
    else if (ax >= VIEW_W) { if (ay < 0) ay = 0; if (ay >= VIEW_H) ay = VIEW_H-1; alarm_wall_pair(ox+VIEW_CELLS_W, oy+ay); }
    else if (ay < 0) alarm_wall_pair(ox+ax*2, oy-1);
    else if (ay >= VIEW_H) alarm_wall_pair(ox+ax*2, oy+VIEW_H);
  }
}

static void draw_line_selector(int x, int y)
{
  int i, yy, a = ACC_BORDER;

  /* Repaint the whole panel every time.  This is important because the
     selection chevrons move between rows and otherwise leave stale glyphs. */
  acc_fill(x, y, 8, 15, ' ', ACC_BG);
  acc_put(x, y, 218, a);
  acc_put(x + 7, y, 191, a);
  acc_put(x + 1, y, 196, a);
  acc_text(x + 2, y, "Line", ACC_HEADING, 4);
  acc_put(x + 6, y, 196, a);
  for (i = 1; i < 14; ++i) {
    acc_put(x, y + i, 179, a);
    acc_put(x + 7, y + i, 179, a);
  }
  acc_put(x, y + 14, 192, a);
  for (i = 1; i < 7; ++i)
    acc_put(x + i, y + 14, 196, a);
  acc_put(x + 7, y + 14, 217, a);

  for (i = 0; i < MAX_LINES; ++i) {
    int swatch = ACC_ATTR(0, lines[i].colour);
    yy = y + 2 + i * 3;
    acc_put(x + 2, yy, 219, swatch);
    acc_put(x + 3, yy, 219, swatch);
    acc_put(x + 4, yy, 219, swatch);
    if (line_armed && i == active_line) {
      acc_put(x + 1, yy, 16, ACC_ATTR(acc_appearance.background, lines[i].colour));
      acc_put(x + 5, yy, 17, ACC_ATTR(acc_appearance.background, lines[i].colour));
    } else {
      acc_put(x + 1, yy, ' ', ACC_BG);
      acc_put(x + 5, yy, ' ', ACC_BG);
    }
    acc_put(x + 3, yy + 1, '1' + i,
            ACC_ATTR(acc_appearance.background, lines[i].colour));
  }
  if (gift_ready)
    acc_put(x + 3, y + 11, MG_TRAIN,
            ACC_ATTR(acc_appearance.background, lines[active_line].colour));
  if (bridge_ready) {
    acc_put(x + 3, y + 12, MG_H,
            ACC_ATTR(WATER_BG, lines[active_line].colour));
    acc_put(x + 4, y + 12, MG_H,
            ACC_ATTR(WATER_BG, lines[active_line].colour));
  }
}

static void draw_status(int x, int y)
{
  char b[80];
  sprintf(b, "Delivered %d   Stations %d%s",
          score, station_count, paused ? "   PAUSED" : "");
  acc_fill(x, y, VIEW_CELLS_W + 4, 1, ' ', ACC_BG);
  acc_text(x, y, b, ACC_HEADING, VIEW_CELLS_W + 4);
  if (warning_station >= 0 || game_over) {
    int right_edge = x + VIEW_CELLS_W + 3; /* right edge of outer brick pair */
    acc_text(right_edge - 8, y, "OVERLOAD!", ACC_ATTR(0, 12), 9);
  }
}

static void draw_buttons(int x, int y, int w, int h, int focus)
{
  int by = y + h - 3;
  char lb[12];
  acc_button(x + 3, by, " Retry ", focus == 1);
  acc_button(x + 11, by, paused ? " Resume " : " Pause ", focus == 2);
  acc_button(x + 20, by, " Prev ", focus == 3);
  acc_button(x + 27, by, " Next ", focus == 4);
  sprintf(lb, "Level %d", level);
  acc_text(x + 34, by, lb, ACC_HEADING, 9);
  acc_button(x + w - 10, by, " Exit ", focus == 5);
}

static void unload_and_load(METRO_LINE *ln, int ti, int station)
{
  METRO_STATION *s = &stations[station];
  int dest = s->type, i, room, n;
  if (ln->pax[ti][dest] > 0) {
    score += ln->pax[ti][dest];
    ln->pax[ti][dest] = 0;
  }
  room = TRAIN_CAP - total_train_pax(ln, ti);
  for (i = 0; i < 3 && room > 0; ++i) {
    if (i == s->type || !line_has_type(ln, i))
      continue;
    n = s->wait[i];
    if (n > room)
      n = room;
    if (n > 0) {
      s->wait[i] -= n;
      ln->pax[ti][i] += n;
      room -= n;
    }
  }
}

static int station_for_path_pos(METRO_LINE *ln, int pos)
{
  int i;
  for (i = 0; i < ln->count; ++i)
    if (ln->stop_path[i] == pos)
      return ln->station[i];
  return -1;
}

static void respace_trains(METRO_LINE *ln)
{
  int ti, cycle;
  if (ln->path_count < 2 || ln->train_count < 1)
    return;
  cycle = (ln->path_count - 1) * 2;
  for (ti = 0; ti < ln->train_count; ++ti)
    ln->train_phase[ti] = (cycle * ti) / ln->train_count;
}

static void add_train_to_line(int li)
{
  METRO_LINE *ln = &lines[li];
  int ti;
  if (ln->count < 2 || ln->path_count < 2 || ln->train_count >= MAX_TRAINS)
    return;
  ti = ln->train_count++;
  memset(ln->pax[ti], 0, sizeof(ln->pax[ti]));
  respace_trains(ln);
}

static void step_trains(METRO_LINE *ln)
{
  int ti, cycle, oldpos, newpos, st;
  if (ln->count < 2 || ln->path_count < 2 || ln->train_count < 1)
    return;
  cycle = (ln->path_count - 1) * 2;
  if (cycle <= 0)
    return;
  for (ti = 0; ti < ln->train_count; ++ti) {
    oldpos = train_path_pos(ln, ti);
    ln->train_phase[ti] = (ln->train_phase[ti] + 1) % cycle;
    newpos = train_path_pos(ln, ti);
    if (newpos != oldpos) {
      st = station_for_path_pos(ln, newpos);
      if (st >= 0)
        unload_and_load(ln, ti, st);
    }
  }
}

static void metro_warning_beeps(void)
{
  int i;
  for (i = 0; i < 3; ++i) {
    unsigned divisor = (unsigned)(1193180UL / 330UL);
    unsigned char old = (unsigned char)inp(0x61);
    outp(0x43, 0xB6);
    outp(0x42, divisor & 0xFF);
    outp(0x42, divisor >> 8);
    outp(0x61, old | 3);
    metro_delay(45);
    outp(0x61, old & 0xFC);
    metro_delay(35);
  }
}

static int station_served_by_all_lines(int s)
{
  int li;
  for (li = 0; li < MAX_LINES; ++li)
    if (!route_contains(&lines[li], s))
      return 0;
  return 1;
}

static void update_gift(void)
{
  int i, eligible = 0;
  for (i = 0; i < station_count; ++i) {
    if (total_wait(i) >= crowd_limit() - 1 && station_served_by_all_lines(i)) {
      eligible = 1;
      break;
    }
  }
  if (eligible && !gift_latched && !gift_ready) {
    gift_ready = 1;
    gift_latched = 1;
  } else if (!eligible) {
    gift_latched = 0;
  }
}

static void use_train_gift(void)
{
  if (!gift_ready)
    return;
  if (lines[active_line].count < 2 || lines[active_line].train_count >= MAX_TRAINS)
    return;
  add_train_to_line(active_line);
  gift_ready = 0;
}

static void spawn_passenger(void)
{
  int s, d, tries = 0;
  if (station_count < 2)
    return;
  s = rand() % station_count;
  do {
    d = rand() % 3;
    ++tries;
  } while (d == stations[s].type && tries < 10);
  if (d != stations[s].type && total_wait(s) < 30)
    ++stations[s].wait[d];
}

static int simulate_tick(void)
{
  int i, changed = 0, old_warning = warning_station, old_gift = gift_ready;
  warning_station = -1;
  if (paused || game_over)
    return 0;
  ++sim_ticks;
  if (sim_ticks - last_passenger_spawn >= passenger_delay()) {
    spawn_passenger();
    last_passenger_spawn = sim_ticks;
    changed = 1;
  }
  if (sim_ticks - last_station_spawn >= station_delay()) {
    add_random_station();
    last_station_spawn = sim_ticks;
    changed = 1;
  }
  if ((sim_ticks & 3UL) == 0) {
    for (i = 0; i < MAX_LINES; ++i)
      step_trains(&lines[i]);
    changed = 1;
  }
  for (i = 0; i < station_count; ++i) {
    if (total_wait(i) > crowd_limit()) {
      if (!stations[i].warning_sent) {
        metro_warning_beeps();
        stations[i].warning_sent = 1;
      }
      if (warning_station < 0)
        warning_station = i;
      ++stations[i].crowd_ticks;
      if (stations[i].crowd_ticks > 180) {
        game_over = 1;
        failed_station = i;
        break;
      }
    } else {
      if (stations[i].crowd_ticks > 0)
        --stations[i].crowd_ticks;
      if (stations[i].crowd_ticks == 0)
        stations[i].warning_sent = 0;
    }
  }
  update_gift();
  if (warning_station != old_warning || gift_ready != old_gift)
    changed = 1;
  return changed;
}

static int find_station_world(int wx, int wy)
{
  int i;
  for (i = 0; i < station_count; ++i)
    if (stations[i].x == wx && stations[i].y == wy)
      return i;
  return -1;
}

static int find_station_screen(int ox, int oy, int mx, int my)
{
  int wx, wy;
  if (mx < ox || mx >= ox + VIEW_CELLS_W || my < oy || my >= oy + VIEW_H)
    return -1;
  wx = pan_x + (mx - ox) / 2;
  wy = pan_y + (my - oy);
  return find_station_world(wx, wy);
}

static int choose_station_exit(int s, int *nx, int *ny)
{
  int vx = stations[s].x - pan_x, vy = stations[s].y - pan_y;
  int dx[4], dy[4], n = 0, i;
  /* Prefer an inward direction based on the station's viewport position. */
  if (vx <= VIEW_W/3) { dx[n]=1; dy[n++]=0; }
  else if (vx >= (VIEW_W*2)/3) { dx[n]=-1; dy[n++]=0; }
  if (vy <= VIEW_H/3) { dx[n]=0; dy[n++]=1; }
  else if (vy >= (VIEW_H*2)/3) { dx[n]=0; dy[n++]=-1; }
  if (n==0) { dx[n]=1; dy[n++]=0; }
  { static const int ax[4]={1,-1,0,0}, ay[4]={0,0,1,-1};
    int j,k,dup;
    for(j=0;j<4;j++){dup=0;for(k=0;k<n;k++)if(dx[k]==ax[j]&&dy[k]==ay[j])dup=1;if(!dup){dx[n]=ax[j];dy[n++]=ay[j];}}
  }
  for(i=0;i<n;i++){int tx=stations[s].x+dx[i],ty=stations[s].y+dy[i];if(tx>=0&&tx<WORLD_W&&ty>=0&&ty<WORLD_H&&!route_cell_used(tx,ty)&&find_station_world(tx,ty)<0){*nx=tx;*ny=ty;return 1;}}
  return 0;
}

static int path_add_step(METRO_LINE *ln, int nx, int ny);

static void path_begin_at_station(int li, int s)
{
  METRO_LINE *ln = &lines[li];
  if (route_contains(ln, s)) {
    if (ln->count < 1 || ln->station[ln->count - 1] != s)
      return;
  } else if (ln->count > 0)
    return;

  if (ln->count == 0) {
    ln->path_count = 1;
    ln->path_x[0] = (unsigned char)stations[s].x;
    ln->path_y[0] = (unsigned char)stations[s].y;
    ln->station[0] = s;
    ln->stop_path[0] = 0;
    ln->count = 1;
  }
  draw_start_path = ln->path_count;
  draw_bridge_used = 0;
  bridge_crossing_active = 0;
  draw_active = 1;
  route_cursor_x = stations[s].x;
  route_cursor_y = stations[s].y;
}

static int path_add_step(METRO_LINE *ln, int nx, int ny)
{
  int lx, ly;
  if (ln->path_count <= 0 || ln->path_count >= MAX_PATH)
    return 0;
  if (nx < 0 || nx >= WORLD_W || ny < 0 || ny >= WORLD_H)
    return 0;
  lx = ln->path_x[ln->path_count - 1];
  ly = ln->path_y[ln->path_count - 1];
  if (nx == lx && ny == ly)
    return 1;
  if (!((nx == lx && (ny == ly - 1 || ny == ly + 1)) ||
        (ny == ly && (nx == lx - 1 || nx == lx + 1))))
    return 0;
  if (terrain[ny][nx] == 2) {
    if (!bridge_ready)
      return 0;
    bridge_ready = 0;
    draw_bridge_used = 1;
  }
  ln->path_x[ln->path_count] = (unsigned char)nx;
  ln->path_y[ln->path_count] = (unsigned char)ny;
  ++ln->path_count;
  route_cursor_x = nx;
  route_cursor_y = ny;
  return 1;
}

static void path_finish_at_station(int li, int s)
{
  METRO_LINE *ln = &lines[li];
  if (!draw_active || ln->count >= MAX_ROUTE || route_contains(ln, s))
    return;
  if (ln->path_count <= 1)
    return;
  if ((int)ln->path_x[ln->path_count - 1] != stations[s].x ||
      (int)ln->path_y[ln->path_count - 1] != stations[s].y) {
    if (!path_add_step(ln, stations[s].x, stations[s].y))
      return;
  }
  ln->station[ln->count] = s;
  ln->stop_path[ln->count] = ln->path_count - 1;
  ++ln->count;
  if (ln->count == 2 && ln->train_count == 0) {
    ln->train_count = 1;
    ln->train_phase[0] = 0;
    memset(ln->pax[0], 0, sizeof(ln->pax[0]));
  }
  respace_trains(ln);
  selected_destination = s;
  draw_active = 0;
  draw_bridge_used = 0;
  bridge_crossing_active = 0;
}

static void try_auto_connect(int li)
{
  int i, dx, dy;
  METRO_LINE *ln = &lines[li];
  if (!draw_active)
    return;
  for (i = 0; i < station_count; ++i) {
    if (route_contains(ln, i))
      continue;
    dx = stations[i].x - route_cursor_x;
    if (dx < 0) dx = -dx;
    dy = stations[i].y - route_cursor_y;
    if (dy < 0) dy = -dy;
    if (dx + dy == 1) {
      path_finish_at_station(li, i);
      return;
    }
  }
}

static void cancel_draw(int li)
{
  METRO_LINE *ln = &lines[li];
  if (!draw_active)
    return;
  ln->path_count = draw_start_path;
  if (draw_bridge_used)
    bridge_ready = 1;
  draw_bridge_used = 0;
  bridge_crossing_active = 0;
  draw_active = 0;
}

static void select_station(int s)
{
  if (s < 0 || s >= station_count)
    return;
  if (draw_active)
    cancel_draw(active_line);
  selected_station = s;
  selected_destination = -1;
}

static int station_visible(int s)
{
  return s >= 0 && s < station_count &&
         world_visible_x(stations[s].x) && world_visible_y(stations[s].y);
}

static int station_before(int a, int b)
{
  if (stations[a].y != stations[b].y) return stations[a].y < stations[b].y;
  return stations[a].x < stations[b].x;
}

static int first_station_sorted(void)
{
  int i,b=0;
  for(i=1;i<station_count;i++)if(station_before(i,b))b=i;
  return station_count?b:-1;
}

static void ensure_station_visible(int s)
{
  if(s<0||s>=station_count)return;
  if(stations[s].x<pan_x)pan_x=stations[s].x;
  else if(stations[s].x>=pan_x+VIEW_W)pan_x=stations[s].x-VIEW_W+1;
  if(stations[s].y<pan_y)pan_y=stations[s].y;
  else if(stations[s].y>=pan_y+VIEW_H)pan_y=stations[s].y-VIEW_H+1;
  if(pan_x<0)pan_x=0;if(pan_y<0)pan_y=0;
  if(pan_x>WORLD_W-VIEW_W)pan_x=WORLD_W-VIEW_W;
  if(pan_y>WORLD_H-VIEW_H)pan_y=WORLD_H-VIEW_H;
}

static void cycle_visible_station(int direction)
{
  int order[MAX_STATIONS],i,j,t,pos=-1;
  if(station_count<=0)return;
  for(i=0;i<station_count;i++)order[i]=i;
  for(i=0;i<station_count-1;i++)for(j=i+1;j<station_count;j++)if(station_before(order[j],order[i])){t=order[i];order[i]=order[j];order[j]=t;}
  for(i=0;i<station_count;i++)if(order[i]==selected_station){pos=i;break;}
  if(pos<0)pos=direction>0?-1:0;
  pos=(pos+direction+station_count)%station_count;
  select_station(order[pos]);
  ensure_station_visible(order[pos]);
}

static void begin_selected_route_if_needed(void)
{
  if (!draw_active && selected_station >= 0)
    path_begin_at_station(active_line, selected_station);
}

static void pan_map(int dx, int dy)
{
  pan_x += dx;
  pan_y += dy;
  if (pan_x < 0) pan_x = 0;
  if (pan_y < 0) pan_y = 0;
  if (pan_x > WORLD_W - VIEW_W) pan_x = WORLD_W - VIEW_W;
  if (pan_y > WORLD_H - VIEW_H) pan_y = WORLD_H - VIEW_H;
}

static void route_step(int dx, int dy)
{
  if (!draw_active)
    return;
  if (path_add_step(&lines[active_line], route_cursor_x + dx, route_cursor_y + dy))
    try_auto_connect(active_line);
  if (route_cursor_x < pan_x) pan_x = route_cursor_x;
  if (route_cursor_x >= pan_x + VIEW_W) pan_x = route_cursor_x - VIEW_W + 1;
  if (route_cursor_y < pan_y) pan_y = route_cursor_y;
  if (route_cursor_y >= pan_y + VIEW_H) pan_y = route_cursor_y - VIEW_H + 1;
}

static void center_on_station(int s)
{
  if (s < 0 || s >= station_count)
    return;
  pan_x = stations[s].x - VIEW_W / 2;
  pan_y = stations[s].y - VIEW_H / 2;
  if (pan_x < 0) pan_x = 0;
  if (pan_y < 0) pan_y = 0;
  if (pan_x > WORLD_W - VIEW_W) pan_x = WORLD_W - VIEW_W;
  if (pan_y > WORLD_H - VIEW_H) pan_y = WORLD_H - VIEW_H;
}

static void failure_animation(int ox, int oy)
{
  static const int bg[6] = {15, 12, 14, 9, 13, 15};
  int k;
  center_on_station(failed_station);
  for (k = 0; k < 6; ++k) {
    board_flash_bg = bg[k];
    draw_map(ox, oy);
    metro_delay(65);
  }
  board_flash_bg = -1;
  draw_map(ox, oy);
}

static void change_level(int delta)
{
  int n = level + delta;
  if (n < 1) n = 25;
  if (n > 25) n = 1;
  level = n;
  reset_game();
}

int main(int argc, char **argv)
{
  int w = 75, h = 23, x, y, ox, oy, sx, sy;
  int key = 0, mx = 0, my = 0, focus = -1, last_focus = -2;
  int mb = 0, last_left = 0, s, redraw = 1;
  unsigned long t;

  if (acc_help(argc, argv, "!METRO",
               "A minimalist text-mode metro-network strategy prototype."))
    return 0;
  if (!acc_begin(argv[0], "Metro", 0))
    return 1;

  metro_font(1);
  srand((unsigned)acc_ticks());
  x = (acc_cols - w) / 2;
  y = (acc_rows - h) / 2;
  sx = x + 3;
  oy = y + 4;
  ox = x + 15; /* tightened one character left */
  sy = oy - 1; /* Lines title aligns with the top brick row */
  reset_game();
  acc_box(x, y, w, h, "Metro");

  while (key != 27) {
    if (redraw) {
      draw_status(ox - 2, y + 2); /* aligned with first outer brick column */
      draw_line_selector(sx, sy);
      draw_map(ox, oy);
      draw_buttons(x, y, w, h, focus);
      redraw = 0;
      last_focus = focus;
    }
    if (focus != last_focus) {
      draw_buttons(x, y, w, h, focus);
      last_focus = focus;
    }

    if (kbhit()) {
      key = acc_key();
      if (key == 27) {
        if (draw_active) {
          cancel_draw(active_line);
          redraw = 1;
        } else if (selected_station >= 0) {
          selected_station = -1;
          selected_destination = -1;
          redraw = 1;
        }
        key = 0;
      } else if (key == 17 || key == 256 + 0x6B) {
        key = 27;
      } else if (!game_over && (key == '1' || key == '2' || key == '3')) {
        int chosen = key - '1';
        if (pending_train_gift) { active_line = chosen; use_train_gift(); pending_train_gift = 0; }
        else {
          if (draw_active) cancel_draw(active_line);
          active_line = chosen;
          line_armed = 1;
          selected_destination = -1;
          selected_station = first_station_sorted();
          if (selected_station >= 0) ensure_station_visible(selected_station);
          focus = -1;
        }
        redraw = 1;
        key = 0;
      } else if (!game_over && (key == 't' || key == 'T')) {
        if (gift_ready) pending_train_gift = 1;
        redraw = 1;
        key = 0;
      } else if (!game_over && (key == 'g' || key == 'G')) {
        if (warning_station >= 0) center_on_station(warning_station);
        redraw = 1; key = 0;
      } else if (!game_over && key == ' ') {
        paused = !paused;
        redraw = 1;
        key = 0;
      } else if (key == 256 + 0x45) {
        paused = !paused;
        redraw = 1;
        key = 0;
      } else if (key == 256 + 0x3F) {
        reset_game();
        redraw = 1;
        key = 0;
      } else if (key == 256 + 0x73) {
        change_level(-1);
        redraw = 1;
        key = 0;
      } else if (key == 256 + 0x74) {
        change_level(1);
        redraw = 1;
        key = 0;
      } else if (!game_over && (key == 256 + 0x48 || key == 256 + 0x50 ||
                 key == 256 + 0x4B || key == 256 + 0x4D)) {
        int dx = 0, dy = 0;
        if (key == 256 + 0x48) dy = -1;
        else if (key == 256 + 0x50) dy = 1;
        else if (key == 256 + 0x4B) dx = -1;
        else dx = 1;
        if (line_armed && (draw_active || (selected_station >= 0 && selected_destination < 0))) {
          begin_selected_route_if_needed();
          route_step(dx, dy);
        } else
          pan_map(dx, dy);
        redraw = 1;
        key = 0;
      } else if (!game_over && line_armed && (key == 9 || key == 271)) {
        cycle_visible_station(key == 271 ? -1 : 1);
        focus = -1;
        redraw = 1;
        key = 0;
      } else if (key == 13) {
        if (focus == 1) reset_game();
        else if (focus == 2) paused = !paused;
        else if (focus == 3) change_level(-1);
        else if (focus == 4) change_level(1);
        else if (focus == 5) key = 27;
        if (key != 27) {
          redraw = 1;
          key = 0;
        }
      }
    }

    if (acc_mouse_present) {
      acc_mouse(&mx, &my, &mb);
      if ((mb & 1) && !last_left) {
        if (my == y && (mx == x + w - 5 || mx == x + w - 4)) {
          key = 27;
        } else if (my == y + h - 3) {
          if (mx >= x + 3 && mx < x + 9) {
            reset_game(); focus = 1; redraw = 1;
          } else if (mx >= x + 11 && mx < x + 18) {
            paused = !paused; focus = 2; redraw = 1;
          } else if (mx >= x + 20 && mx < x + 26) {
            change_level(-1); focus = 3; redraw = 1;
          } else if (mx >= x + 27 && mx < x + 33) {
            change_level(1); focus = 4; redraw = 1;
          } else if (mx >= x + w - 10) {
            key = 27;
          }
        } else if (!game_over && mx >= sx + 1 && mx <= sx + 5) {
          if (my == sy + 11 && gift_ready) {
            use_train_gift();
          } else {
            if (draw_active)
              cancel_draw(active_line);
            if (my == sy + 2) active_line = 0;
            else if (my == sy + 5) active_line = 1;
            else if (my == sy + 8) active_line = 2;
            line_armed = 1;
            selected_destination = -1;
            selected_station = first_station_sorted();
            if (selected_station >= 0) ensure_station_visible(selected_station);
          }
          redraw = 1;
        } else if (!game_over && line_armed) {
          s = find_station_screen(ox, oy, mx, my);
          if (s >= 0) {
            select_station(s);
            focus = -1;
            redraw = 1;
          }
        }
      }
      last_left = mb & 1;
    }

    t = acc_ticks();
    if (t != last_tick) {
      last_tick = t;
      if (simulate_tick())
        redraw = 1;
      if (game_over && !failure_shown) {
        failure_shown = 1;
        failure_animation(ox, oy);
        acc_notice("Metro", "Overcrowding has shut down the metro.\nThe people are not happy, try again.");
      }
    }
  }

  metro_font(0);
  acc_end_screen();
  acc_end();
  return 0;
}
