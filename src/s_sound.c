/* tolbat s_sound.c */
#include "doomdef.h"
#include "music.h"

#define EXTERN_BUFFER_SIZE (EXTERNALQUADS*32/2)
sfxchannel_t    sfxchannels[SFXCHANNELS];

int             finalquad;          /* the last quad mixed by update. */
                                    
int             sfxvolume = 132;    /* range 0 - 255 */
int             musicvolume = 100;  /* range 0 - 255 */
int             oldsfxvolume = 132; /* to detect transition to sound off */
int             oldmusvolume = 100; /* mirror musicvolume for transition tracking */

int				soundtics;			/* time spent mixing sounds */
int				soundstarttics;		/* time S_Update started */

int				sfxsample;			/* the sample about to be output */
									/* by S_WriteOutSamples */

/*			 MUSIC VARIABLES */

sfx_t           *instruments[256];	/* pointers to all patches */

channel_t       music_channels[10];	/* master music channel list */

int             musictime;			/* internal music time, follows samplecount */
int             next_eventtime;		/* when next event will occur */


unsigned char   *music;				/* pointer to current music data */
unsigned char   *music_start;		/* current music start pointer */
unsigned char   *music_end;			/* current music end pointer */
unsigned char	*music_memory;		/* current location of cached music */

int             samples_per_midiclock;	/* multiplier for midi clocks */

int				musictics = 0;

int             curmid, curlp;      /* last music id/looping requested for volume on/off */
static int      last_sfx_start[NUMSFX];	/* gametic when each throttled SFX last started */

#define abs(x) ((x)<0 ? -(x) : (x))


/*
==================
=
= S_Init
=
==================
*/

void S_Init(void)
{
	int		i,l;
	int	lump, end;
	int instnum;

/*				SFX */
	
	for (i=1 ; i < NUMSFX ; i++)
	{
		l = W_CheckNumForName(S_sfx[i].name);
		if (l != -1)
			S_sfx[i].md_data = W_POINTLUMPNUM(l);
	}	

/*				MUSIC */

 	D_memset(instruments, 0, 256 * 4);
 	lump = W_GetNumForName("inststrt");			/* get available instruments[] */
 	end	= W_GetNumForName("instend");
 	while (lump != end)
 	{
 		instnum = (lumpinfo[lump].name[1]-'0')*100
 				+ (lumpinfo[lump].name[2]-'0')*10
 				+ (lumpinfo[lump].name[3]-'0')
 				+ (lumpinfo[lump].name[0] == 'P' ? 128 : 0);
 		instruments[instnum] = (sfx_t *) (wadfileptr + lumpinfo[lump].filepos);
 		lump++;
 	}
 
 	/* hack test */

	music_memory = 0;
	music = 0;
	D_memset(music_channels, 0, sizeof(music_channels));
	musictime = 0;
	next_eventtime = 0;


	S_Clear();		   
}


/*
==================
=
= S_Clear
=
==================
*/


void S_Clear (void)
{
	D_memset (sfxchannels,0,sizeof(sfxchannels));
	D_memset (last_sfx_start,0,sizeof(last_sfx_start));
	D_memset (soundbuffer,0,0x4000);
}

static void S_ClearSfxLane (void)
{
    short   *dest;
    int     count;

    dest = ((short *)soundbuffer) + 1;
    count = EXTERN_BUFFER_SIZE;

    while (count--)
    {
        *dest = 0;
        dest += 2;
    }
}

static void S_ClearMusicLane (void)
{
    short   *dest;
    int     count;

    dest = (short *)soundbuffer;
    count = EXTERN_BUFFER_SIZE;

    while (count--)
    {
        *dest = 0;
        dest += 2;
    }
}

/*
==================
=
= S_RestartSounds
=
==================
*/

void S_RestartSounds (void)
{
}

/*
==================
=
= S_IsMusicSuppressedAmbient
=
= Returns true for low-value ambient sounds that should not play over music.
=
==================
*/

static boolean S_IsMusicSuppressedAmbient(int sound_id)
{
	switch (sound_id)
	{
	case sfx_bgact:
	case sfx_dmact:
	case sfx_posact:
		return true;

	default:
		break;
	}

	return false;
}


/*
==================
=
= S_ShouldThrottleMusicSfx
=
= Limits rapid repeats of selected combat sounds while music is playing.
= The first sound still plays; only repeats inside the delay window are skipped.
=
==================
*/

static boolean S_ShouldThrottleMusicSfx(int sound_id)
{
	int delay;
	int elapsed;

	if (!music || !musicvolume)
		return false;

	if (sound_id <= sfx_None || sound_id >= NUMSFX)
		return false;

	switch (sound_id)
	{
	case sfx_firsht:
	case sfx_sgtatk:
	case sfx_claw:
	case sfx_firxpl:
	case sfx_dmpain:
	case sfx_popain:
	case sfx_barexp:
	case sfx_slop:
		delay = 2;
		break;

	default:
		return false;
	}

	elapsed = gametic - last_sfx_start[sound_id];

	if (last_sfx_start[sound_id] && elapsed < delay)
		return true;

	last_sfx_start[sound_id] = gametic;
	return false;
}



/*
==================
=
= S_StartSound
=
==================
*/

void S_StartSound(mobj_t *origin, int sound_id)
{
#ifdef JAGUAR
	sfxchannel_t	*channel, *newchannel;
	int 			i;
	int			currentquad;
	int 		dist_approx;
	player_t 	*player;
	int 		dx, dy;
	short		vol;
	sfxinfo_t	*sfx;

	if (!sfxvolume)
		return;

	if (sound_id <= sfx_None || sound_id >= NUMSFX)
		return;
		
/* */
/* spatialize */
/* */
	player = &players[consoleplayer];

	if (!origin || origin == player->mo)
		vol = 127;
	else
	{
		dx = abs(origin->x - player->mo->x);
		dy = abs(origin->y - player->mo->y);
		dist_approx = dx + dy - ((dx < dy ? dx : dy) >> 1);
		vol = dist_approx >> 20;
		if (vol > 127)
			return;		/* too far away */
		vol = 127 - vol;
	}

/* Get sound effect data pointer */
	sfx = &S_sfx[sound_id];
	if (!sfx->md_data)
		return;

	if (music && musicvolume && S_IsMusicSuppressedAmbient(sound_id))
		return;

	if (S_ShouldThrottleMusicSfx(sound_id))
		return;

	currentquad = samplecount >> 3;
	if (finalquad < currentquad)
		finalquad = currentquad;

	newchannel = NULL;
	
/* reject sounds started at the same instant and singular sounds */
	for (channel=sfxchannels,i=0 ; i<SFXCHANNELS ; i++,channel++)
	{
		if (channel->sfx == sfx)
		{
			if (channel->startquad == finalquad)
			{
				return;		/* exact sound allready started */
			}

			if (sfx->singularity)
			{
				newchannel = channel;	/* overlay this	 */
				goto gotchannel;
			}
		}
		if (channel->origin == origin)
		{	/* cut off whatever was coming from this origin */
			newchannel = channel;
			goto gotchannel;
		}
		
		if (channel->stopquad <= finalquad)
			newchannel = channel;	/* this is a dead channel, ok to reuse */
	}

/* if there weren't any dead channels, try to kill an equal or lower */
/* priority channel */

	if (!newchannel)
	{
		for (newchannel=sfxchannels,i=0 ; i<SFXCHANNELS ; i++, newchannel++)
			if (newchannel->sfx->priority >= sfx->priority)
				goto gotchannel;
		return;		/* couldn't override a channel */
	}


/* */
/* fill in the new values */
/* */
gotchannel:
	newchannel->sfx = sfx;
	newchannel->origin = origin;
	newchannel->startquad = finalquad;
	newchannel->stopquad = finalquad + (sfx->md_data->samples>>2);
	newchannel->source = (int *)&sfx->md_data->data;	
	newchannel->volume = vol * (short)sfxvolume;
#endif
}


/*
===================
=
= S_UpdateSounds
=
===================
*/

extern	int	sfx_start;
extern	int music_dspcode;

void S_UpdateSounds(void)
{
#ifdef JAGUAR

	int st;

/* */
    /* If sound was just turned off, clear out the buffer.
       Do NOT early-return — we keep music logic running while SFX is muted. */
/* */
	if (!sfxvolume)
	{
		if (oldsfxvolume)
		{
            /* sound just turned off, clear buffer */
			oldsfxvolume = 0;
			S_ClearSfxLane();
		}
	}
	else
	{
		if (!oldsfxvolume)
			finalquad = (samplecount >> 3) - 100;	/* don't mix lots of junk */
		oldsfxvolume = sfxvolume;
	}

	/* Music mute/unmute transitions (runs regardless of SFX state) */
	if (!musicvolume)
	{
		if (oldmusvolume)
		{
            /* music just turned off */
            oldmusvolume = 0;
									   
            S_StopSong();
        }

		/* make sure finalquad doesn't fall behind while music off */
        if (finalquad < (samplecount >> 3) - EXTERNALQUADS)
        {
			finalquad = (samplecount >> 3) - 100;
			sfxsample = finalquad << 3;
		}
    }
    else
    {
        if (!oldmusvolume)
            S_StartSong(curmid, curlp); /* just turned on, restart music */
        oldmusvolume = musicvolume;
																 
									
												
   
    }
	
	soundstarttics = samplecount;		/* for timing calculations */

/* */
/* run the mixing in parallel on the dsp */
/*	 */

										   
	if (music)
	{
		if (!musictime)
			musictime = next_eventtime = samplecount + EXTERN_BUFFER_SIZE/2;

		while (samplecount - musictime > EXTERN_BUFFER_SIZE)
		{
			musictime += EXTERN_BUFFER_SIZE;
			next_eventtime += EXTERN_BUFFER_SIZE;
		}

		st = samplecount;
		DSPFunction (&music_dspcode);
		musictics = samplecount - st; /* how long it took to generate the music */
	}


/* SFX mixing is independent of music; run when SFX are enabled */
	if (sfxvolume)
	{
		st = samplecount;
		dspfinished = 0x1234;
		dspcodestart = (int)&sfx_start;
		DSPFunction(&sfx_start);
		soundtics = samplecount - st;
	}
#endif
}

void S_StartSong(int music_id, int looping)
{
	int lump;

    curmid = music_id;
    curlp = looping;
	next_eventtime = musictime;
	musictime = 0;
	samples_per_midiclock = 0;
    if (musicvolume)
    {
        lump = W_GetNumForName(S_music[music_id].name);
        music_memory = music = 
            (unsigned char *) W_CacheLumpNum(lump, PU_STATIC);
        music_start = looping ? music : 0;
        music_end = (unsigned char *) music + lumpinfo[lump].size ;
        sfxsample = musictime; /* Align SFX with music start */
    }
    else
    {
        music_memory = music = 0;
        sfxsample = 0;
        S_ClearMusicLane();
    }
}

void S_StopSong(void)
{
    if (music)
    {
        Z_Free (music_memory);
        music = 0;							/* prevent the DSP from running */
    }

	S_ClearMusicLane();
}
