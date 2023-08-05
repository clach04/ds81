/*
   ds81 - Nintendo DS ZX81 emulator.

   Copyright (C) 2006  Ian Cowburn <ianc@noddybox.co.uk>
   
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
   $Id$
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nds.h>
#include <fat.h>

#include "framebuffer.h"
#include "gui.h"
#include "keyboard.h"
#include "z80.h"
#include "zx81.h"
#include "tapes.h"
#include "config.h"
#include "textmode.h"
#include "monitor.h"
#include "snapshot.h"

#include "splashimg_bin.h"
#include "rom_font_bin.h"

#include "ds81_debug.h"

#ifndef DS81_VERSION
#define DS81_VERSION "DEV " __TIME__ "/" __DATE__
#endif


/* ---------------------------------------- STATIC DATA
*/
static const char *main_menu[]=
	{
	    "Reset ZX81",
	    "Select Tape",
	    "Configure",
	    "Map Joypad to Keys",
	    "Machine Code Monitor",
#ifndef DS81_DISABLE_FAT
	    "Save Memory Snapshot",
	    "Load Memory Snapshot",
	    "Save Joypad/Key State",
	    "Load Joypad/Key State",
#endif
	    "Cancel",
	    NULL
	};

typedef enum
{
    MenuReset,
    MenuSelectTape,
    MenuConfigure,
    MenuMapJoypad,
    MenuMonitor,
#ifndef DS81_DISABLE_FAT
    MenuSaveSnapshot,
    MenuLoadSnapshot,
    MenuSaveMappings,
    MenuLoadMappings
#endif
} MenuOpt;


/* Backgrounds
*/
static int	main_bitmap_bg;
static int	main_text_overlay_bg;
static int	sub_bitmap_bg;
static int	sub_text_overlay_bg;

/* ---------------------------------------- IRQ FUNCS
*/

/* ---------------------------------------- DISPLAY FUNCS
*/
static void VBlankFunc(void)
{
    scanKeys();
}

static void Splash(void)
{
    static char scroller[]=
    {
    	"                   "
	"Welcome to DS81, a ZX81 emulator for the Ninetendo DS.  "
	"You can safely ignore this message.  I was just bored for half an "
	"hour.  And no retro game is complete without a side-scroller...  "
	"Thanks to Slay Radio, Ladytron, the Genki Rockets, the High "
	"Voltage SID Collection and Retro Gamer for coding fuel."
    };

    static const char *text[]=
    {
    	"DS81 \177 2006 Ian C",
	" ",
	"ZX81 ROM \177 1981",
	"Nine Tiles Networks LTD",
	" ",
	"PRESS A TO CONTINUE",
	" ",
	"http://www.noddybox.co.uk/",
	" ",
	" ",
	" ",
	" ",
	"Checking for FAT device...",
	NULL
    };

    sImage img;
    int f;
    int y;
    int res=FALSE;
    int scr_x=0;

    ZX81SuspendDisplay();
    ZX81DisplayString("10 rem " DS81_VERSION "\n"
		      "20 print \"%the zx81 is ace%\"\n"
		      "30 goto 20");

    bgSetScale(sub_text_overlay_bg, 0x80, 0x80);

    TM_printf(0,11,"%-18.18s",scroller);

    FB_Clear();

    loadPCX(splashimg_bin,&img);

    FB_Blit(&img,0,0,1);

    y = 10;

    for(f=0;text[f];f++)
    {
	FB_Centre(text[f],y,COL_WHITE,COL_TRANSPARENT);
	y += 8;
    }

    y += 8;

#ifndef DS81_DISABLE_FAT
    res = fatInitDefault();
#endif

    if (res)
    {
	ZX81EnableFileSystem(TRUE);
	SNAP_Enable(TRUE);

	FB_Centre("Found a FAT device.",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("If you place .P tape files in",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("the top directory or ZX81SNAP",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("then you should be able to load",y,
				COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("GAME.P with the command",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("LOAD \"GAME\"",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;
    }
    else
    {
	ZX81EnableFileSystem(FALSE);
	SNAP_Enable(FALSE);

	FB_Centre("Sorry, but you don't have a",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("supported FAT device.",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("Only the internal tape",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;

	FB_Centre("files can be used.",y,COL_WHITE,COL_TRANSPARENT);
	y += 8;
    }

    while(!(keysDown() & KEY_A))
    {
	swiWaitForVBlank();

	if (++scr_x == 8)
	{
	    size_t l = sizeof scroller;
	    char c;

	    scr_x = 0;

	    c = scroller[0];
	    memmove(scroller,scroller+1,l-2);
	    scroller[l-2] = c;

	    TM_printf(0,11,"%-18.18s",scroller); 
	}

	bgSetScroll(sub_text_overlay_bg, scr_x << 8, 0);
	bgUpdate();
    }

    bgSetScale(sub_text_overlay_bg, 0x100, 0x100);
    bgSetScroll(sub_text_overlay_bg, 0, 0);
    bgUpdate();

    TM_Cls();

    ZX81ResumeDisplay();
}


/* ---------------------------------------- JOYPAD MAPPING
*/
static void MapJoypad(void)
{
    SoftKeyEvent ev;
    SoftKey pad = NUM_SOFT_KEYS;
    int done = FALSE;
    char text[256];

    SK_DisplayKeyboard();

    ZX81SuspendDisplay();

    ZX81DisplayString("press the joypad button you want\n"
		      "to define and then the ZX81 key\n"
		      "you want to use.\n\n"
		      "press on the config banner to\n"
		      "finish.");

    while(!done)
    {
	while(SK_GetBareEvent(&ev))
	{
	    if (ev.pressed)
	    {
	    	if (ev.key==SK_ABOUT || ev.key==SK_CONFIG)
		{
		    done = true;
		}
	    }
	    else
	    {
		if (ev.key>=SK_PAD_UP && ev.key<=SK_PAD_SELECT)
		{
		    pad = ev.key;

		    /* Now, just how dumb was making % the inverse on/off...
		    */
		    sprintf(text,"defining\n  %%%s%%",SK_KeyName(pad));
		    ZX81DisplayString(text);
		}

		if (ev.key<=SK_SPACE && pad!=NUM_SOFT_KEYS)
		{
		    sprintf(text,"mapped\n  %%%s%%\nto\n  %%%s%%",
		    			SK_KeyName(pad),SK_KeyName(ev.key));
		    ZX81DisplayString(text);

		    SK_DefinePad(pad,ev.key);

		    pad = NUM_SOFT_KEYS;
		}
	    }
	}

	swiWaitForVBlank();
    }

    ZX81ResumeDisplay();
}


/* ---------------------------------------- MAIN
*/
int main(int argc, char *argv[])
{
    Z80 *z80;

    powerOn(POWER_ALL_2D);

    /* Set up main screen for ZX81 and load the ROM character data
    */
    videoSetMode(MODE_3_2D);

    vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
    vramSetBankB(VRAM_B_MAIN_BG_0x06020000);

    main_bitmap_bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 2, 0);
    main_text_overlay_bg = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 0, 1);

    bgSetPriority(main_bitmap_bg, 1);
    bgSetPriority(main_text_overlay_bg, 0);

    BG_PALETTE[0] = RGB15(31,31,31);
    BG_PALETTE[1] = RGB15(0,0,0);

    dmaCopy(rom_font_bin,(void *)BG_TILE_RAM(1),rom_font_bin_size);

    /* Set up the sub-screen for rotation (basically for use as a framebuffer).
       Now overlaid with a text screen for the monitor (I thought a bitmapped
       printing routine would needlessly slow down the monitor when watching
       the ZX81 run).  Having said the overlay is currently a rotation map
       for some pointless frippery!  Still be quicker though.
    */
    videoSetModeSub(MODE_4_2D);

    vramSetBankC(VRAM_C_SUB_BG_0x06200000);

    sub_bitmap_bg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 1, 0);
    sub_text_overlay_bg = bgInitSub(2, BgType_Rotation, BgSize_R_256x256, 4, 0);

    bgSetPriority(sub_bitmap_bg, 1);
    bgSetPriority(sub_text_overlay_bg, 0);

    /* Tell 'framebuffer' routines to use this
    */
    FB_Init((uint16*)BG_BMP_RAM_SUB(1), BG_PALETTE_SUB);

    /* Set up lower screen text overlay
    */
    FB_LoadASCIITiles((uint16*)BG_TILE_RAM_SUB(0));
    TM_Init((uint16*)BG_MAP_RAM_SUB(4),32,32,TRUE);

    /* Set up interrupts and timers 
    */
    irqInit();
    irqSet(IRQ_VBLANK,VBlankFunc);
    irqEnable(IRQ_VBLANK);

    /* All required stuff initialised
    */
    keysSetRepeat(30,15);

    z80 = Z80Init(ZX81ReadMem,
		  ZX81WriteMem,
		  ZX81ReadPort,
		  ZX81WritePort,
		  ZX81ReadDisassem);

    if (!z80)
    {
	GUI_Alert(TRUE,"Failed to initialise\nthe Z80 CPU emulation!");
    }

    ZX81Init((uint16*)BG_MAP_RAM(0), (uint16*)BG_BMP_RAM(2), z80);

    Splash();

    LoadConfig();
    ZX81Reconfigure();

    SK_DisplayKeyboard();

    SK_SetSticky(SK_SHIFT,DS81_Config[DS81_STICKY_SHIFT]);

    if (DS81_Config[DS81_LOAD_DEFAULT_SNAPSHOT])
    {
    	SNAP_Load(z80, "AUTO", SNAP_TYPE_FULL);
    }

    while(1)
    {
	SoftKeyEvent ev;

    	Z80Exec(z80);

	while(SK_GetEvent(&ev))
	{
	    switch(ev.key)
	    {
	    	case SK_ABOUT:
	    	case SK_CONFIG:
		    if (ev.pressed)
		    {
			switch(GUI_Menu(main_menu))
			{
			    case MenuReset:
				ZX81Reset(z80);
				break;

			    case MenuSelectTape:
			    	SelectTape();
				break;

			    case MenuConfigure:
				GUI_Config();
				SK_SetSticky(SK_SHIFT,
					     DS81_Config[DS81_STICKY_SHIFT]);
				ZX81Reconfigure();
				break;

			    case MenuMapJoypad:
				MapJoypad();
			    	break;

			    case MenuMonitor:
			    	MachineCodeMonitor(z80);
				break;

#ifndef DS81_DISABLE_FAT
			    case MenuSaveSnapshot:
			    	SNAP_Save(z80, SNAP_TYPE_FULL);
			    	break;

			    case MenuLoadSnapshot:
			    	SNAP_Load(z80, NULL, SNAP_TYPE_FULL);
			    	break;

			    case MenuSaveMappings:
			    	SNAP_Save(z80, SNAP_TYPE_KEYBOARD);
			    	break;

			    case MenuLoadMappings:
			    	SNAP_Load(z80, NULL, SNAP_TYPE_KEYBOARD);
			    	break;
#endif
			}

			SK_DisplayKeyboard();
		    }
		    break;

	    	default:
		    ZX81HandleKey(ev.key,ev.pressed);
		    break;
	    }
	}
    }

    return 0;
}
