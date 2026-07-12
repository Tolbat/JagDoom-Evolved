/* o_main.c -- options menu */

#include "doomdef.h"
#include "p_local.h"
#include "st_main.h"

#define MOVEWAIT    5
#define ITEMSPACE   40
#define SLIDEWIDTH  90

extern  int     cx, cy;
extern  int     sfxvolume;      /* range from 0 to 255 */
extern  int     musicvolume;    /* range from 0 to 255 */
extern  int     controltype;    /* 0 to 5 */

#define NUMSPECTRESTYLES 10

int     spectrestyle = 0;
int     spectredvcmd = 0x41260F09;

int spectredvcmds[NUMSPECTRESTYLES] =
{
    0x41260F09,     /* Green Goblin */
    0x41102F09,     /* Purple People Eater */
    0x40404F09,     /* Blue Beast */
    0x00024F09,     /* Neon Nemesis */
    0x44D04F09,     /* Swamp */
	0x41A02F09,     /* Halloween */
	0x40E00F09,     /* Toxic */
	0x41600F09,     /* Slimer */
	0x41115F09,     /* Disco */
	0x46402F09      /* Black*/
};

char spectrenames[NUMSPECTRESTYLES][16] =
{
    "Goblin",
    "Magenta",
    "Sapphire",
    "Neon",
    "Swamp",
	"Pumpkin",
	"Toxic",
	"Slimer",
	"Disco",
	"Void"
};

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
    widescrn,
    soundvol,
    musicvol,
    spectre,
    controls,
    NUMMENUITEMS
} menupos_t;

menupos_t   cursorpos;

typedef struct
{
    int     x;
    int     y;
    boolean hasslider;
    char    name[32];
} menuitem_t;

menuitem_t menuitem[NUMMENUITEMS];

void O_SetSpectreStyle (void)
{
    if (spectrestyle < 0)
        spectrestyle = NUMSPECTRESTYLES - 1;
    if (spectrestyle >= NUMSPECTRESTYLES)
        spectrestyle = 0;

    spectredvcmd = spectredvcmds[spectrestyle];
}

typedef struct
{
    int curval;
    int maxval;
} slider_t;

slider_t slider[NUMMENUITEMS];

int     cursorframe, cursorcount;
int     movecount;
boolean menudirty;

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

char anamorphic[2][4] =
{
    "Off",
    "On"
};

boolean initmathtbl = true;
boolean anamorphicview;
int     stretch;
fixed_t stretchscale;

void O_SetStretch (void)
{
    stretch = anamorphicview ? 28*8 : 22*8;
    stretchscale = anamorphicview ? 183501 : 144179;
}

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
    EraseBlock(menuitem[widescrn].x + 180, menuitem[widescrn].y, 70, 20);
    print(menuitem[widescrn].x + 180, menuitem[widescrn].y, anamorphic[anamorphicview ? 1 : 0]);

    EraseBlock(menuitem[spectre].x + 150, menuitem[spectre].y, 120, 20);
    print(menuitem[spectre].x + 150, menuitem[spectre].y, spectrenames[spectrestyle]);

    EraseBlock(menuitem[controls].x + 40, menuitem[controls].y + 20, 90, 80);
    print(menuitem[controls].x + 40, menuitem[controls].y + 20, buttona[controltype]);
    print(menuitem[controls].x + 40, menuitem[controls].y + 40, buttonb[controltype]);
    print(menuitem[controls].x + 40, menuitem[controls].y + 60, buttonc[controltype]);
/*  IN_DrawValue(30, 20, controltype); */
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
    O_SetButtonsFromControltype ();
	
/* the eeprom has set anamorphicview, so set render stretch from that */
    O_SetStretch ();

/* cache all needed graphics */
    o_cursor1 = W_CacheLumpName ("M_SKULL1",PU_STATIC);
    o_cursor2 = W_CacheLumpName ("M_SKULL2",PU_STATIC);
    o_slider = W_CacheLumpName ("O_SLIDER", PU_STATIC);
    o_slidertrack = W_CacheLumpName ("O_STRACK", PU_STATIC);

    l = W_GetNumForName ("CHAR_065");
    for (i = 0; i < 52; i++)
        uchar[i] = W_CacheLumpNum(l+i, PU_STATIC);

/* initialize variables */
    cursorcount = 0;
    cursorframe = 0;
    cursorpos = 0;
    movecount = 0;
    menudirty = true;

/* anamorphic widescreen */
	D_strncpy(menuitem[widescrn].name, "Widescreen", 11);
	menuitem[widescrn].x = 45;
	menuitem[widescrn].y = 28;
	menuitem[widescrn].hasslider = false;

/* sound effects volume */
	D_strncpy(menuitem[soundvol].name, "Sfx Vol", 8);
	menuitem[soundvol].x = 45;
	menuitem[soundvol].y = 52;
	menuitem[soundvol].hasslider = true;
	slider[soundvol].maxval = 16;
	slider[soundvol].curval = 16 * sfxvolume / 255;

/* music volume */
	D_strncpy(menuitem[musicvol].name, "Mus Vol", 8);
	menuitem[musicvol].x = 45;
	menuitem[musicvol].y = 72;
	menuitem[musicvol].hasslider = true;
	slider[musicvol].maxval = 16;
	slider[musicvol].curval = 16 * musicvolume / 255;

/* spectre style */
	D_strncpy(menuitem[spectre].name, "Spectre", 8);
	menuitem[spectre].x = 45;
	menuitem[spectre].y = 96;
	menuitem[spectre].hasslider = false;
	O_SetSpectreStyle ();

/* controls */
	D_strncpy(menuitem[controls].name, "Controls", 9);
	menuitem[controls].x = 85;
	menuitem[controls].y = 116;
	menuitem[controls].hasslider = false;
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
    int     buttons, oldbuttons;

    buttons = ticbuttons[playernum];
    oldbuttons = oldticbuttons[playernum];

    if ( (buttons & BT_OPTION) && !(oldbuttons & BT_OPTION) )
    {
        cursorpos = 0;
        player->automapflags ^= AF_OPTIONSACTIVE;
        if (player->automapflags & AF_OPTIONSACTIVE)
        {
            DoubleBufferSetup ();
            menudirty = true;
        }
        else
            WriteEEProm ();     /* save new settings */
    }
    if ( !(player->automapflags & AF_OPTIONSACTIVE) )
        return;

/* clear buttons so game player isn't moving around */
    ticbuttons[playernum] &= BT_OPTION;  /* leave option status alone */

    if (playernum != consoleplayer)
        return;

/* animate skull */
    if (++cursorcount == 4)
    {
        cursorframe ^= 1;
        cursorcount = 0;
    }

/* check for movement */
    if (! (buttons & (JP_UP|JP_DOWN|JP_LEFT|JP_RIGHT) ) )
        movecount = 0;      /* move immediately on next press */
    else
    {
        if (movecount == MOVEWAIT)
            movecount = 0;      /* repeat move */
        if (++movecount == 1)
        {
            if (buttons & JP_DOWN)
            {
                cursorpos++;
                if (cursorpos == NUMMENUITEMS)
                    cursorpos = 0;
            }

            if (buttons & JP_UP)
            {
                cursorpos--;
                if (cursorpos == -1)
                    cursorpos = NUMMENUITEMS-1;
            }

            if (buttons & JP_RIGHT)
            {
                if (menuitem[cursorpos].hasslider)
                {
                    if (slider[cursorpos].curval < slider[cursorpos].maxval)
                    {
                        slider[cursorpos].curval++;
                        if (cursorpos == soundvol)
                        {
                            sfxvolume = 255*slider[soundvol].curval / slider[soundvol].maxval;
                            S_StartSound (NULL, sfx_pistol);
                        }
                        else if (cursorpos == musicvol)
                        {
                            musicvolume = 255*slider[musicvol].curval / slider[musicvol].maxval;
                        }
                        menudirty = true;
                    }
                }
				
				else if (cursorpos == widescrn)
                {
                    anamorphicview = true;
                    initmathtbl = true;
                    O_SetStretch ();
                    menudirty = true;
                }
				
                else if (cursorpos == spectre)
                {
                    spectrestyle++;
                    O_SetSpectreStyle ();
                    menudirty = true;
                }
                else if (cursorpos == controls)
                {
                    controltype++;
                    if (controltype == NUMCONTROLOPTIONS)
                        controltype = NUMCONTROLOPTIONS-1;
                    O_SetButtonsFromControltype ();
                    menudirty = true;
                }
            }

            if (buttons & JP_LEFT)
            {
                if (menuitem[cursorpos].hasslider)
                {
                    if (slider[cursorpos].curval > 0)
                    {
                        slider[cursorpos].curval--;
                        if (cursorpos == soundvol)
                        {
                            sfxvolume = 255*slider[soundvol].curval / slider[soundvol].maxval;
                            S_StartSound (NULL, sfx_pistol);
                        }
                        else if (cursorpos == musicvol)
                        {
                            musicvolume = 255*slider[musicvol].curval / slider[musicvol].maxval;
                        }
                        menudirty = true;
                    }
                }
                
				else if (cursorpos == widescrn)
                {
                    anamorphicview = false;
                    initmathtbl = true;
                    O_SetStretch ();
                    menudirty = true;
                }
				
				else if (cursorpos == spectre)
                {
                    spectrestyle--;
                    O_SetSpectreStyle ();
                    menudirty = true;
                }
                else if (cursorpos == controls)
                {
                    controltype--;
                    if (controltype == -1)
                        controltype = 0;
                    O_SetButtonsFromControltype ();
                    menudirty = true;
                }
            }
        }
    }
}

void O_Drawer (void)
{
    int     i;
    int     offset;

    if (menudirty)
    {
        EraseBlock(0, 0, 320, 200);

    /* Draw menu */
        print(104, 10, "Options");

        for (i = 0; i < NUMMENUITEMS; i++)
        {
            print(menuitem[i].x, menuitem[i].y, menuitem[i].name);

            if (menuitem[i].hasslider == true)
            {
                DrawJagobj(o_slidertrack, menuitem[i].x + 112,
                    menuitem[i].y + 2);
                offset = (slider[i].curval * SLIDEWIDTH) /
                    slider[i].maxval;
                DrawJagobj(o_slider, menuitem[i].x + 117 + offset,
                    menuitem[i].y + 2);
            }
        }

    /* Draw control info */
        print(menuitem[controls].x + 10, menuitem[controls].y + 20, "A");
        print(menuitem[controls].x + 10, menuitem[controls].y + 40, "B");
        print(menuitem[controls].x + 10, menuitem[controls].y + 60, "C");

        O_DrawControl();

        menudirty = false;
    }

/* Erase old and Draw new cursor frame */
    EraseBlock(16, 24, o_cursor1->width, 176);
    if (cursorframe)
        DrawJagobj(o_cursor1, 20, menuitem[cursorpos].y - 2);
    else
        DrawJagobj(o_cursor2, 20, menuitem[cursorpos].y - 2);

    UpdateBuffer ();
}