/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

#define MOVEWAIT    5
#define ITEMSPACE   40
#define SLIDEWIDTH  90

extern  int     cx, cy;
extern  int     sfxvolume;      /* range from 0 to 255 */
extern  int     controltype;    /* 0 to 5 */
extern  boolean rotary_control_enabled;

extern void print (int x, int y, char *string);
extern void IN_DrawValue(int x,int y,int value);

/* action buttons can be set to BT_A, BT_B, or BT_C */
/* strafe and use should be set to the same thing */
extern  unsigned    BT_ATTACK;
extern  unsigned    BT_USE;
extern  unsigned    BT_STRAFE;
extern  unsigned    BT_SPEED;

typedef enum
{
    SFU,
    SUF,
    FSU,
    FUS,
    USF,
    UFS,
    NUMCONTROLOPTIONS
} control_t;

typedef enum
{
    widescrn,           /* Page 1 */
    soundvol,
    musicvol,
    controls,
    placeholder_one,    /* Page 2 */
    placeholder_two,
    rotary_toggle,
    placeholder_four,
    future_use_one,     /* Page 3 */
    future_use_two,
    future_use_three,
    future_use_four,
    NUMMENUITEMS
} menupos_t;

menupos_t   cursorpos;
int         currentpage;    /* Added: 0 = first page, 1 = second page, 2 = third page */

typedef struct
{
    int     x;
    int     y;
    boolean hasslider;
    char    name[20];
} menuitem_t;

menuitem_t menuitem[12];  /* Modified: Increased size to 12 for 3 pages with 4 items each */
 
typedef struct
{
    int curval;
    int maxval;
} slider_t;

slider_t slider[12];  /* Modified: Increased size to 12 to match menuitem count */

int     cursorframe, cursorcount;
int     movecount;

jagobj_t    *uchar[52];
jagobj_t    *o_cursor1, *o_cursor2;
jagobj_t    *o_slider, *o_slidertrack;

char buttona[NUMCONTROLOPTIONS][8] =
		{"Speed","Speed","Fire","Fire","Use","Use"};
char buttonb[NUMCONTROLOPTIONS][8] = 
		{"Fire","Use ","Speed","Use","Speed","Fire"};
char buttonc[NUMCONTROLOPTIONS][8] =
		{"Use","Fire","Use","Speed","Fire","Speed"};

unsigned configuration[NUMCONTROLOPTIONS][3] =
{
    {BT_A, BT_B, BT_C},
    {BT_A, BT_C, BT_B},
    {BT_B, BT_A, BT_C},
    {BT_C, BT_A, BT_B},
    {BT_B, BT_C, BT_A},
    {BT_C, BT_B, BT_A} 
};

char anamorphic[2][4] =	{"Off","On"};
char onoff[2][4] = {"Off","On"};

boolean rotary_control_enabled = false;

boolean initmathtbl = true;

boolean anamorphicview;
int stretch;
fixed_t stretchscale;

void O_SetButtonsFromControltype (void)
{
    BT_SPEED = configuration[controltype][0];
    BT_ATTACK = configuration[controltype][1];
    BT_USE = configuration[controltype][2]; 
    BT_STRAFE = configuration[controltype][2]; 
}

/* */
/* Draw control value */
/* */
void O_DrawControl(void)
{
    /* Modified: Only draw on page 1 to keep pages 2 and 3 blank */
    if (currentpage == 0)
    {
        EraseBlock(menuitem[widescrn].x + 40, menuitem[widescrn].y + 20, 60, 20);
        print(menuitem[widescrn].x + 40, menuitem[widescrn].y + 20, anamorphic[anamorphicview ? 1 : 0]);

        EraseBlock(menuitem[controls].x + 40, menuitem[controls].y + 20, 90, 80);
        print(menuitem[controls].x + 40, menuitem[controls].y + 20, buttona[controltype]);
        print(menuitem[controls].x + 40, menuitem[controls].y + 40, buttonb[controltype]);
        print(menuitem[controls].x + 40, menuitem[controls].y + 60, buttonc[controltype]);
    }

/*  IN_DrawValue(30, 20, controltype); */
    
    O_SetButtonsFromControltype();
    stretch = anamorphicview ? 28*8 : 22*8;
    stretchscale = anamorphicview ? 183501 : 144179;
}

/*
===============
=
= O_Init
=
===============
*/
void O_Init (void)
{
    int i, l;

/* the eeprom has set controltype, so set buttons from that */
    O_SetButtonsFromControltype();

/* the eeprom has widescreen setting */
    stretch = anamorphicview ? 28*8 : 22*8;
    stretchscale = anamorphicview ? 183501 : 144179;

/* cache all needed graphics */
    o_cursor1 = W_CacheLumpName("M_SKULL1", PU_STATIC);
    o_cursor2 = W_CacheLumpName("M_SKULL2", PU_STATIC);
    o_slider = W_CacheLumpName("O_SLIDER", PU_STATIC);
    o_slidertrack = W_CacheLumpName("O_STRACK", PU_STATIC);

    l = W_GetNumForName("CHAR_065");
    for (i = 0; i < 52; i++)
        uchar[i] = W_CacheLumpNum(l+i, PU_STATIC);

/*  initialize variables */
    cursorcount = 0;
    cursorframe = 0;
    cursorpos = 0;
    currentpage = 0;  /* Added: Initialize currentpage to start on page 1 */

/* Page 1: Original options (functionality unchanged*/
/*  chillywilly: anamorphic widescreen */
    D_strncpy(menuitem[widescrn].name, "Widescreen", 10);
	menuitem[widescrn].x = 85;
	menuitem[widescrn].y = 40;
	menuitem[widescrn].hasslider = false;

    D_strncpy(menuitem[soundvol].name, "Sfx Vol", 7); /* Fixed CEF */
	menuitem[soundvol].x = 45;
	menuitem[soundvol].y = 80;
	menuitem[soundvol].hasslider = true;

 	slider[soundvol].maxval = 16;
	slider[soundvol].curval = 16*sfxvolume/255;

/*  chillywilly: music volume */
    D_strncpy(menuitem[musicvol].name, "Mus Vol", 7);
	menuitem[musicvol].x = 45;
	menuitem[musicvol].y = 100;
	menuitem[musicvol].hasslider = true;

 	slider[musicvol].maxval = 16;
	slider[musicvol].curval = 16*musicvolume/255;

    D_strncpy(menuitem[controls].name, "  Controls", 10); /* Fixed CEF */
	menuitem[controls].x = 85;
	menuitem[controls].y = 120;
	menuitem[controls].hasslider = false;

/* Page 2: Placeholder items with even spacing and aligned x positions */
    D_strncpy(menuitem[placeholder_one].name, "hud options", 15);
    menuitem[placeholder_one].x = 45; /* Aligned to left side */
    menuitem[placeholder_one].y = 40;
    menuitem[placeholder_one].hasslider = false;

    D_strncpy(menuitem[placeholder_two].name, "mouse control", 15);
    menuitem[placeholder_two].x = 45; /* Aligned to left side */
    menuitem[placeholder_two].y = 70;
    menuitem[placeholder_two].hasslider = false;

	D_strncpy(menuitem[rotary_toggle].name, "rotary control", 14);
	menuitem[rotary_toggle].x = 45;
	menuitem[rotary_toggle].y = 100;
	menuitem[rotary_toggle].hasslider = false;

    D_strncpy(menuitem[placeholder_four].name, "placeholder", 16);
    menuitem[placeholder_four].x = 45; /* Aligned to left side */
    menuitem[placeholder_four].y = 140;
    menuitem[placeholder_four].hasslider = false;

/* Page 3: Placeholder items with even spacing and aligned x positions */
    D_strncpy(menuitem[future_use_one].name, "future use", 14);
    menuitem[future_use_one].x = 45; /* Aligned to left side */
    menuitem[future_use_one].y = 40;
    menuitem[future_use_one].hasslider = false;

    D_strncpy(menuitem[future_use_two].name, "future use two", 14);
    menuitem[future_use_two].x = 45; /* Aligned to left side */
    menuitem[future_use_two].y = 70;
    menuitem[future_use_two].hasslider = false;

    D_strncpy(menuitem[future_use_three].name, "future use three", 16);
    menuitem[future_use_three].x = 45; /* Aligned to left side */
    menuitem[future_use_three].y = 100;
    menuitem[future_use_three].hasslider = false;

    D_strncpy(menuitem[future_use_four].name, "future use four", 15);
    menuitem[future_use_four].x = 45; /* Aligned to left side */
    menuitem[future_use_four].y = 130;
    menuitem[future_use_four].hasslider = false;
}

/*
==================
=
= O_Control
=
= Button bits can be eaten by clearing them in ticbuttons[playernum]
==================
*/
void O_Control (player_t *player)
{
    int buttons, oldbuttons;
    
    buttons = ticbuttons[playernum];
    oldbuttons = oldticbuttons[playernum];
    
    if ((buttons & BT_OPTION) && !(oldbuttons & BT_OPTION))
    {
        /* Fixed: Ensure menu state is initialized when entering from title screen */
        cursorpos = 0;  
        currentpage = 0;
        player->automapflags ^= AF_OPTIONSACTIVE;
        if (player->automapflags & AF_OPTIONSACTIVE)
            DoubleBufferSetup();
        else
            WriteEEProm(); /* save new settings */
    }
    if (!(player->automapflags & AF_OPTIONSACTIVE))
        return;

/* clear buttons so game player isn't moving around */
    ticbuttons[playernum] &= BT_OPTION; /* leave option status alone */

    if (playernum != consoleplayer)
        return;
        
/* animate skull */
    if (++cursorcount == 4)
    {
        cursorframe ^= 1;
        cursorcount = 0;
    }

/* check for movement */
    if (!(buttons & (JP_UP | JP_DOWN | JP_LEFT | JP_RIGHT)))
        movecount = 0; /* move immediately on next press */
    else
    {
	if (buttons & JP_RIGHT)
	{
		if (menuitem[cursorpos].hasslider)
		{
			slider[cursorpos].curval++;
			if (slider[cursorpos].curval > slider[cursorpos].maxval)
				slider[cursorpos].curval = slider[cursorpos].maxval;

			if (cursorpos == soundvol)
			{
				sfxvolume = 255 * slider[soundvol].curval / slider[soundvol].maxval;
				S_StartSound(NULL, sfx_pistol);
			}
			else if (cursorpos == musicvol)
			{
				musicvolume = 255 * slider[musicvol].curval / slider[musicvol].maxval;
			}
		}

		if (cursorpos == rotary_toggle)
		{
			rotary_control_enabled = true;
		}
	}

	if (buttons & JP_LEFT)
	{
		if (menuitem[cursorpos].hasslider)
		{
			slider[cursorpos].curval--;
			if (slider[cursorpos].curval < 0)
				slider[cursorpos].curval = 0;

			if (cursorpos == soundvol)
			{
				sfxvolume = 255 * slider[soundvol].curval / slider[soundvol].maxval;
				S_StartSound(NULL, sfx_pistol);
			}
			else if (cursorpos == musicvol)
			{
				musicvolume = 255 * slider[musicvol].curval / slider[musicvol].maxval;
			}
		}

    if (cursorpos == rotary_toggle)
    {
        rotary_control_enabled = false;
    }
}


        if (movecount == MOVEWAIT)
            movecount = 0; /* repeat move */
        if (++movecount == 1)
        {
            /* Define page boundaries for 4 items per page */
            int first_item = currentpage * 4;
            int last_item = first_item + 3;

            if (buttons & JP_DOWN)
            {
                cursorpos++;
                if (cursorpos > last_item)
                {
                    /* Cycle through three pages */
                    currentpage = (currentpage + 1) % 3; /* 0 -> 1 -> 2 -> 0 */
                    cursorpos = currentpage * 4; /* First item of new page */
                }
            }
            if (buttons & JP_UP)
            {
                /* Check if we're on the first item of the page */
                if (cursorpos == first_item)
                {
                    /* Wrap to the previous page */
                    currentpage = (currentpage - 1 + 3) % 3; /* 2 -> 1 -> 0 -> 2 */
                    cursorpos = (currentpage * 4) + 3; /* Last item of new page */
                }
                else
                {
                    /* Move up within the same page */
                    cursorpos--;
                    /* Ensure cursorpos doesn't go below the first item of the page */
                    if (cursorpos < first_item)
                        cursorpos = first_item;
                }
            }
            if (buttons & JP_RIGHT)
            {
                if (cursorpos == controls)
                {
                    controltype++;
                    if(controltype >= NUMCONTROLOPTIONS)
                        controltype = NUMCONTROLOPTIONS - 1;
                }
                else if (cursorpos == widescrn)
                {
                    anamorphicview = true;
                    initmathtbl = true;
                }
            }
            if (buttons & JP_LEFT)
            {
                if (cursorpos == controls)
                {
                    controltype--;
                     if(controltype < 0)
                        controltype = 0; 
                }
                else if (cursorpos == widescrn)
                {
                    anamorphicview = false;
                    initmathtbl = true;
                }
            }
        }
    }
}

void O_Drawer (void)
{
    int i;
    int offset;
    int first_item; /* Added: For multi-page rendering */
    int relative_cursor; /* Added: For correct cursor positioning */

/* Modified: Clear a wider area for consistent background on all pages */
    EraseBlock(16, 40, 300, 200); /* Covers menu items and sliders */

/* Draw cursor using relative position within the current page */
    first_item = currentpage * 4;
    relative_cursor = cursorpos - first_item;
    if (cursorframe)
        DrawJagobj(o_cursor1, 20, menuitem[first_item + relative_cursor].y - 2);
    else
        DrawJagobj(o_cursor2, 20, menuitem[first_item + relative_cursor].y - 2);

/* Draw menu */
    print(104, 10, "Options");

/* Draw only the items for the current page */
    for (i = currentpage * 4; i <= currentpage * 4 + 3; i++)
    {
        print(menuitem[i].x, menuitem[i].y, menuitem[i].name);  
		
	/* Print ON/OFF below the 'rotary controls' label */
    if (i == rotary_toggle && currentpage == 1)
        print(menuitem[i].x, menuitem[i].y + 20, onoff[rotary_control_enabled ? 1 : 0]);

        if (menuitem[i].hasslider == true)
        {
            DrawJagobj(o_slidertrack , menuitem[i].x + 112, menuitem[i].y + 2);
            offset = (slider[i].curval * SLIDEWIDTH) / slider[i].maxval;
            DrawJagobj(o_slider, menuitem[i].x + 117 + offset, menuitem[i].y + 2);
        }

    }

/* Draw control info only on page 1 */
    if (currentpage == 0)
    {
        print(menuitem[controls].x + 10, menuitem[controls].y + 20, "A");
        print(menuitem[controls].x + 10, menuitem[controls].y + 40, "B");
        print(menuitem[controls].x + 10, menuitem[controls].y + 60, "C");
    }

    O_DrawControl();

/* debug stuff */
#if 0
    cx = 30;
    cy = 40;
    D_printf("Speed = %d", BT_SPEED);
    cy = 60;
    D_printf("Use/Strafe = %d", BT_SPEED);
    cy = 80;
    D_printf("Fire = %d", BT_SPEED);
#endif
/* end of debug stuff */

    UpdateBuffer();
}