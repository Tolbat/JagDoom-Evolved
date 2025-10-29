/* s_sound.c */
#include "doomdef.h"
#include "music.h"
#ifndef EXTERN_BUFFER_SIZE
#ifdef EXTERNALQUADS
#define EXTERN_BUFFER_SIZE (EXTERNALQUADS * 32)
#else
#define EXTERN_BUFFER_SIZE 0x4000
#endif
#endif

#ifndef EXTERN_BUFFER_SAMPLES
#define EXTERN_BUFFER_SAMPLES (EXTERN_BUFFER_SIZE / 2)
#endif

/* Let d_main.c know whether to wait on DSP this frame (SFX only sets this). */
int dsp_should_wait = 0;

sfxchannel_t    sfxchannels[SFXCHANNELS];

boolean         channelschanged;    /* set by S_StartSound to signal */
                                    /* update to remix speculative samples */

int             finalquad;          /* the last quad mixed by update. */
                                    
int             sfxvolume = 128;    /* range 0 - 255 */
int             musicvolume = 128;  /* range 0 - 255 */
int             oldsfxvolume = 128; /* to detect transition to sound off */
int             oldmusvolume = 128; /* mirror musicvolume for transition tracking */
int				soundtics;			/* time spent mixing sounds */
int				soundstarttics;		/* time S_Update started */

int				sfxsample;			/* the sample about to be output */
									/* by S_WriteOutSamples */

/*			 MUSIC VARIABLES */

sfx_t           *instruments[256];	/* pointers to all patches */

channel_t       music_channels[10];	/* master music channel list */

int             musictime;			/* internal music time, follows samplecount */
int             next_eventtime;		/* when next event will occur */
static int last_music_id = -1;      /* Remember what to resume after a mute */
static int last_music_loop = 0;
unsigned char   *music;				/* pointer to current music data */
unsigned char   *music_start;		/* current music start pointer */
unsigned char   *music_end;			/* current music end pointer */
unsigned char	*music_memory;		/* current location of cached music */

int             samples_per_midiclock;	/* multiplier for midi clocks */

int				musictics = 0;


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
/*	S_StartSong(1,1); */
  
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
    if (!soundbuffer) return;
    D_memset (sfxchannels,0,sizeof(sfxchannels));
#ifdef EXTERNALQUADS
    D_memset (soundbuffer,0,EXTERNALQUADS*32);
#else
    D_memset (soundbuffer,0,0x4000);
#endif
}

void S_RestartSounds (void)
{
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
	int 		dist_approx;
	player_t 	*player;
	int 		dx, dy;
	short		vol;
	sfxinfo_t	*sfx;

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
    /* Guard: missing/unloaded sample data */
    if (!sfx->md_data)
        return;
	
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
/*	channelschanged = true;   Signal S_UpdateSounds to mix this SFX */
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
	
    dsp_should_wait = 0;   /* default: don't wait this frame unless SFX actually mixed */

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
			S_Clear();
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
			oldmusvolume = 0;
			/* Falling edge: stop music once. (No shared-buffer wipe.) */
			S_StopSong();
		}
	}
	else
	{
		if (!oldmusvolume)
		{
			oldmusvolume = musicvolume;
			/* Rising edge: if music was stopped, resume the last track */
			if (!music && last_music_id >= 0)
				S_StartSong(last_music_id, last_music_loop);
		}
	}

	
	soundstarttics = samplecount;		/* for timing calculations */

/* */
/* run the mixing in parallel on the dsp */
/*	 */

/* Run the mixing in parallel on the DSP */
if (music)
{
    if (!musictime)
        musictime = next_eventtime = samplecount + EXTERN_BUFFER_SAMPLES/2;

    while (samplecount - musictime > EXTERN_BUFFER_SAMPLES)
    {
        musictime     += EXTERN_BUFFER_SAMPLES;
        next_eventtime += EXTERN_BUFFER_SAMPLES;
    }

    st = samplecount;
    DSPFunction(&music_dspcode);
    musictics = samplecount - st;
}

/* SFX mixing is independent of music; only runs if SFX is enabled */
if (sfxvolume)
{
    st = samplecount;
    dspfinished  = 0x1234;
    dspcodestart = (int)&sfx_start;
    DSPFunction(&sfx_start);
    soundtics = samplecount - st;  /* time to mix SFX */
    dsp_should_wait = 1;           /* tell main loop it may wait this frame */
}
#endif
}


void S_StartSong(int music_id, int looping)
{
    int lump;  /* declarations must come before any statements */
    /* Record the current song for auto-resume after mute */
    last_music_id = music_id;
    last_music_loop = looping;
    /* Guard against double-start: stop any previous song cleanly */
    if (music && music_memory)
    {
        S_StopSong();
    }

    /* Keep transition tracker in sync with current volume */
    oldmusvolume = musicvolume;		  
					
    next_eventtime = musictime;
    musictime = 0;
    samples_per_midiclock = 0;

	 
    lump = W_GetNumForName(S_music[music_id].name);
    music_memory = music =
        (unsigned char *) W_CacheLumpNum(lump, PU_STATIC);
    music_start = looping ? music : 0;
    music_end = (unsigned char *) music + lumpinfo[lump].size;

    sfxsample = musictime; /* Align SFX with music start */
}

void S_StopSong(void)
{
    /* If music is already stopped or we never allocated music_memory, exit safely */
    if (!music || !music_memory)
    {
        music = 0;
        return;
    }

    /* Free the music buffer and mark music as stopped so the DSP path won’t run */
    Z_Free(music_memory);
    music_memory = 0;
    music = 0;

    /* Clear the music/SFX external buffer once to prevent stale samples */
#ifdef EXTERNALQUADS
    D_memset(soundbuffer, 0, EXTERNALQUADS * 32);
#else
    D_memset(soundbuffer, 0, 0x4000);
#endif
}
