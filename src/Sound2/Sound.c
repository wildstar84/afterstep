/*
 * Copyright (C) 2007,2008 Jeremy sG <sg at hacksess>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

// Standard "Required" Includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <alsa/asoundlib.h>

// AS Includes
#include "../../configure.h"
#include "../../libAfterStep/asapp.h"
#include "../../libAfterStep/screen.h"  
#include "../../libAfterStep/event.h"
#include "../../libAfterStep/wmprops.h"
#include "../../libAfterConf/afterconf.h"
#include "../../libAfterStep/clientprops.h"
#include "Sound.h"
// Defines
#define MAX_SOUNDS AFTERSTEP_EVENTS_NUM
#define mask_reg MAX_MASK

static char *assoundplayer;
// from libAfterConf
SoundConfig     *CONF = NULL;

// asapp.h ?
void DeadPipe (int);
void GetBaseOptions (const char *filename/* unused */);
void GetOptions (const char *filename);
void HandleEvents();
void DispatchEvent (ASEvent * event);  
void proc_message (send_data_type type, send_data_type *body);

// Sound2 Functions
int as_snd2_init_stg1();
/* JWT:NO LONGER SEEMS TO BE USED: */
#if 0
int as_snd2_init_stg2();
int as_snd2_waveheader(char *WaveFile);
#endif
int as_snd2_playsound(int evend_ID);
void as_snd2_segv_cb(int);
void as_snd2_error();

// ?
Bool (*sound_play) (short) = NULL;

// Global Vars
struct SND2dev SND2devS;
//SND2devP = &SND2devS;
int SNDPlayNow;

int main(int argc,char ** argv)
{
    // Standard AS Stuff
    set_DeadPipe_handler(DeadPipe);
    InitMyApp(CLASS_SOUND,argc,argv,NULL,NULL,0);
    LinkAfterStepConfig();
    
    ConnectX(ASDefaultScr,PropertyChangeMask);
    ConnectAfterStep(mask_reg,0);
    
    // -------
    CONF = CreateSoundConfig();

    LoadBaseConfig(GetBaseOptions);
    LoadConfig("sound",GetOptions);
    
    if (CONF->debug) { fprintf(stdout,"--PCM Device: %s\n",CONF->pcmdevice); }
    /*
    
       Need to Read sounds into memory.
       And also parse out headers, into a related struct
       with the rate, size, etc.
    
    */

    // We're going to catch any seg fault signals from ALSA
    signal(SIGSEGV,as_snd2_segv_cb);

    
    as_snd2_init_stg1();
    // Technically, Stage2 Init should Not be done here.
    // as_snd2_init_stg2();
    
    /*
    
      Add better error control in snd2_init..
      if either error out, should unload sounds from 
      memory before exiting.
    
    */
    
    // Start the event loop
    HandleEvents();
    
    // prolly need additional error checking here.
    // for what should be rare case of HandleEvents breaking.
    return 0;
}

int as_snd2_init_stg1()
{
    // We're going to catch any seg fault signals from ALSA
//:        signal(SIGSEGV,as_snd2_segv_cb);
        
    // Stage 1 Sound2 module Init.
    
    int ErrRet;

    /* JWT:NOW ALLOW FOR USER-DEFINED AUDIO-PLAYING PROGRAM: */
    assoundplayer = PutHome(getenv ("ASSOUNDPLAYER"));
    if (!assoundplayer) {
        assoundplayer = PutHome (SOUNDPLAYER);
        if (assoundplayer && !assoundplayer[0])
            assoundplayer = NULL;
    }

    if (assoundplayer)
        fprintf(stdout,"Stage 1 Init Complete, using ASSOUNDPLAYER=%s=!\n\n", assoundplayer);
    else {
        if (CONF->debug) { fprintf(stdout,"Opening PCM Device.... %s\n",CONF->pcmdevice); }
        // (Returned PCM Handle, ASCII Ident of Handle, Wanted Stream, Open Mode)
        ErrRet = snd_pcm_open(&SND2devS.pcm_handle,CONF->pcmdevice,SND_PCM_STREAM_PLAYBACK,0);
        if (ErrRet <= -1) { fprintf(stdout,"ALSA Error: %i [%s] (quitting!).\n", ErrRet, snd_strerror(ErrRet)); exit(1); }

        if (CONF->debug) { fprintf(stdout,"Allocating Hardware Param Structure....\n"); }

        // (Returned Pointer)
        ErrRet = snd_pcm_hw_params_malloc(&SND2devS.hw_params);
        if (ErrRet <= -1) { fprintf(stdout,"ALSA Error: %i [%s] (quitting!).\n",ErrRet, snd_strerror(ErrRet)); exit(1);  }

        if (CONF->debug) { fprintf(stdout,"Init Hardware Param Structure...\n");}

        // (PCM Handle, Config Space)
        ErrRet = snd_pcm_hw_params_any(SND2devS.pcm_handle,SND2devS.hw_params);
        if (ErrRet <= -1) { fprintf(stdout,"ALSA Error: %i [%s] (quitting!).\n",ErrRet, snd_strerror(ErrRet)); exit(1);  }

        if (CONF->debug) { fprintf(stdout,"Setting Access Type...\n"); }

        // (PCM Handle, Config Space, Access Type)
        ErrRet = snd_pcm_hw_params_set_access(SND2devS.pcm_handle,SND2devS.hw_params,SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ErrRet <= -1) { fprintf(stdout,"ALSA Error: %i [%s] (quitting!).\n",ErrRet, snd_strerror(ErrRet)); exit(1);  }

        if (CONF->debug) { fprintf(stdout,"Setting Sample Format...\n"); }

        // (PCM Handle, Config Space, Format)
        // SND_PCM_FORMAT_S16_LE = Signed 16-bit Little Endian
        // May need to var-ize last part, and detect cpu type on configure.
        ErrRet = snd_pcm_hw_params_set_format(SND2devS.pcm_handle,SND2devS.hw_params,SND_PCM_FORMAT_S16_LE);
        if (ErrRet <= -1) { fprintf(stdout,"ALSA Error: %i [%s] (quitting!).\n",ErrRet, snd_strerror(ErrRet)); exit(1);  }

        if (CONF->debug) { fprintf(stdout,"Stage 1 Init Complete!\n\n"); }
    }

    return 1;
}

/* JWT:NO LONGER SEEMS TO BE USED: */
#if 0
int as_snd2_init_stg2()
{
    // Following Init depends on the file type.
    // since we're setting Sample rate, channel count (mono,stereo), etc.
    
    int SRate, CHcount, dir;
    
    SRate = 22000;
    CHcount = 2;

    if (CONF->debug) { fprintf(stdout,"Setting Sample Rate...\n"); }
    
    // (PCM Handle, Config Space, Sample Rate, Sub Unit Direction)
    snd_pcm_hw_params_set_rate_near(SND2devS.pcm_handle,SND2devS.hw_params,&SRate,&dir);
    
    if (CONF->debug) { fprintf(stdout,"Setting Channel Count...\n"); }
    
    // (PCM Handle, Config Space, Channel Count)
    snd_pcm_hw_params_set_channels(SND2devS.pcm_handle,SND2devS.hw_params,CHcount);
    
    if (CONF->debug) { fprintf(stdout,"Apply Hardware Params...\n"); }
    
    // (PCM Handle, Config Space)
    snd_pcm_hw_params(SND2devS.pcm_handle,SND2devS.hw_params);
    
    if (CONF->debug) { fprintf(stdout,"Free Hardware Space Container...\n"); }
    
    // (Config Space)
    snd_pcm_hw_params_free(SND2devS.hw_params);
    
    if (CONF->debug) { fprintf(stdout,"Stage 2 Init Complete!\n\n"); }
    
    //HandleEvents(SND2devP);
    
    return 1;
}

int as_snd2_waveheader(char *WaveFile)
{
    // Read Wave file header info
    FILE *WaveFileR;
    UCHAR *headBuff;
    int headret;
    
    WaveFileR = fopen(WaveFile,"rb");
    
    headBuff = (UCHAR*)malloc(1024);
    headret = fread(headBuff,44,1,WaveFileR);
    fclose(WaveFileR);
    
    return 1;
}
#endif

int as_snd2_playsound(int event_ID)
{
	if (event_ID < 0 || event_ID >= AFTERSTEP_EVENTS_NUM)
	    return 0;

    // Lets play some sound now!
    
    char *SFbuffer;
    int SNDFileSize, SNDFileFrames, SNDFileRead;
    FILE *SNDFile;
    const char *SndFname;
    char SndFullname[200];
    
    if (CONF->debug)  fprintf(stdout,"***SNDNow: %i: %d\n",SNDPlayNow, event_ID);
    
    if (SNDPlayNow == 1) 
    { 
        if (CONF->debug) fprintf(stdout,"***Already Playing!***\n");
        return 0;
    }

    SndFname = CONF->sounds[event_ID];
    
    /*
      If a sound option isnt defined in the conf file,
      we need to set it to something, else fopen will
      cause a segfault.
    */
    if (SndFname == NULL)  SndFname = "unmapped";
    
    SNDPlayNow = 1;
    
    strcpy(SndFullname,PutHome(CONF->path));
    strcat(SndFullname,"/");
    strcat(SndFullname,SndFname);
    
    //fprintf(stdout,"TEST NAME: %s\n",SndFullname);
    
    // 5 = ICONIFIED
    // 7 = SHADE
    // 9 = STUCK
    //CONF->sounds[7]

	/* JWT:NOW ALLOW FOR USER-DEFINED AUDIO-PLAYING PROGRAM: */
	if (assoundplayer)
		spawn_child (assoundplayer, -1, -1, NULL, None, 0, True, False, SndFullname, NULL);
	else {
		SNDFile = fopen(SndFullname,"rb");
		if (SNDFile == NULL) { fprintf(stdout,"Failed to open Sound File\n"); return 0; }

	    /*
	       Need to Grab Header info to pass into stage 2 Init:
	       Sample Rate, Channels

		THEN continue on (if no errors from stage 2.    
		*/

		snd_pcm_prepare(SND2devS.pcm_handle);    

		if (CONF->debug)  fprintf(stdout,"Play Sound: %s: fid=(%s)\n", SndFname, SndFullname);

		fseek(SNDFile,0,SEEK_END); // We're trying to get the full file size here...
		SNDFileSize = ftell(SNDFile);
		rewind(SNDFile); // Now that we got size, go back to beginning
		fseek(SNDFile,128,SEEK_SET); // we're skipping first 128 bytes. its unplayable (header info)
    
		SNDFileFrames = snd_pcm_bytes_to_frames(SND2devS.pcm_handle,SNDFileSize);
    
		SFbuffer = (char*)malloc(SNDFileSize);
		while ((SNDFileRead = fread(SFbuffer,4,SNDFileFrames,SNDFile)) > 0)
		{
			snd_pcm_writei(SND2devS.pcm_handle,SFbuffer,SNDFileFrames);
		}
    }
//    SNDPlayNow = 0;

    return 1;
}

/* ======== STANDARD AS STUFF ============ */
void HandleEvents()
{   
    ASEvent event;
    Bool has_x_events = False ;
    while (True)
    {
        while((has_x_events = XPending (dpy)) )
        {
            if( ASNextEvent (&(event.x), True) )
            {
                event.client = NULL ;
                setup_asevent_from_xevent( &event );
                DispatchEvent( &event );
            }
        }
        module_wait_pipes_input ( proc_message );
    }
}
void DispatchEvent (ASEvent * event)
{
    SHOW_EVENT_TRACE(event);
    switch (event->x.type)  
    {
        case PropertyNotify:
            handle_wmprop_event (Scr.wmprops, &(event->x));
            break;
    }
}    
void proc_message(send_data_type type, send_data_type *body)
{
    int StateChange, code = -1;
    
    if (type == M_PLAY_SOUND) { show_activity("M_PLY_SND"); } // Need to find whats calling this type

    else if ((type & WINDOW_PACKET_MASK) != 0)
    {
        struct ASWindowData *wd = fetch_window_by_id(body[0]);
        WindowPacketResult res;
        
        ASFlagType old_state = wd ? wd->state_flags:0;
        res = handle_window_packet(type,body,&wd);
        
        if (res == WP_DataCreated)
        {
            code = EVENT_WindowAdded;
            if (CONF->debug)  fprintf(stdout,"---EVENT_WindowAdded(%d)\n", code);
        }
        else if (res == WP_DataChanged)
        {
            if (type == M_FOCUS_CHANGE)
            {	
                show_activity("FOC_CHANGE"); 
                code = EVENT_WindowActivated;
                if (CONF->debug)  fprintf(stdout,"---(focus) EVENT_WindowActivated(%d)\n", code);
                // as_snd2_playsound("focus");
            }
            
            StateChange = wd->state_flags ^ old_state;
            if (StateChange)
            {
                /*
                
                  This needs to be massively simplified.
                  Possibly generating a struct with the numbers,
                  then match those to the sound files.
                  
                  Then, can just pass that number straight to the 
                  play function, and remove a ton of IFs.
                
                */
                if (StateChange & AS_Shaded)
                {
                    //show_activity("STATE: Shade");
                    code = EVENT_WindowShaded;
                    // as_snd2_playsound("shade");
                }
                if (StateChange & AS_Iconic)
                {	
                    //show_activity("STATE: Iconic"); 
                    code = EVENT_WindowIconified;
                    // as_snd2_playsound("iconic");
                }
                if (StateChange & AS_Sticky)
                {	
                    //show_activity("STATE: Sticky");
                    code = EVENT_WindowStuck;
                    // as_snd2_playsound("sticky");
                }
                if (StateChange & AS_MaximizedY)
                {
                    //show_activity("STATE: MaximizedY");
                    code = EVENT_WindowMaximized;
                    // as_snd2_playsound("maximized");
                }
                // Uncertain Flags
                // > libAfterStep/clientprops.h
                if (StateChange & AS_Withdrawn)
                {
                    show_activity("STATE: Withdrawn");
                    code = EVENT_WindowIconified;
                }
                if (StateChange & AS_Dead)
                {
                    show_activity("STATE: Dead");
                    code = EVENT_WindowDestroyed;
                }
                if (StateChange & AS_Layer)
                {
                    show_activity("STATE: Layer");
                }
                if (StateChange & AS_Size)
                {
                    show_activity("STATE: Size");
                }
                
            }
            res = -1;
        }
        else if (res == WP_DataDeleted)
        {	code = EVENT_WindowDestroyed; }
        
    }
    if (type & M_NEW_DESKVIEWPORT)
    {
        // Getting this message when opening new windows/terms.
        show_activity("-STATE: VIEWPORT CHANGE");
        code = EVENT_DeskViewportChanged;
        // as_snd2_playsound("viewport");
    }
    if (type & M_STACKING_ORDER)
    {                          
        // Layering. This and Foc Change occur on menu opening. 
        show_activity("-STATE[2]: Stack Order");
    }
    as_snd2_playsound(code);
    
    SNDPlayNow = 0;
}
void
GetBaseOptions (const char *filename/* unused*/)
{
    START_TIME(started);
        ReloadASEnvironment( NULL, NULL, NULL, False, False );
    SHOW_TIME("BaseConfigParsingTime",started);
}
void
GetOptions (const char *filename)
{
    if (CONF->debug)  fprintf(stdout,"*****GETOPTIONS START(%s)******\n", filename);
    SoundConfig *config = ParseSoundOptions (filename,MyName);

    int i ;
    START_TIME(option_time);
    
    /* Need to merge new config with what we have already :*/
    /* now lets check the config sanity : */
    /* mixing set and default flags : */
    CONF->set_flags |= config->set_flags;

    if( config->pcmdevice != NULL )
    {
        CONF->pcmdevice = mystrdup(config->pcmdevice);
        //set_string( &(CONF->pcmdevice), config->pcmdevice);
    }
    if (CONF->debug) {  fprintf(stdout,"Cfg::PCMDEV: %s\n",CONF->pcmdevice); }
    
    if (config->path != NULL)
    {
        CONF->path = mystrdup(config->path);
    }
    if (config->debug)
    {
        CONF->debug = config->debug;
        config->debug = 0;
    }
        
    for( i = 0 ; i < AFTERSTEP_EVENTS_NUM ; ++i ) 
    {
        if( config->sounds[i] != NULL )
        {
            if (CONF->debug) { fprintf(stdout,"--SOUNDS: [%i]: %s\n",i,config->sounds[i]); }
            set_string( &(CONF->sounds[i]), mystrdup(config->sounds[i]));
        }
    }
    
    
    // This delay option seems... unneeded.
//    if( get_flags(config->set_flags, AUDIO_SET_DELAY) )
//    {
//        Config->delay = config->delay;
//        fprintf(stdout,"---DELAY: %i",Config->delay);
//    }

    DestroySoundConfig(config);
    SHOW_TIME("Config parsing",option_time);
    if (CONF->debug) { fprintf(stdout,"******GETOPTIONS END*****\n"); }
}
 
void as_snd2_segv_cb(int sig)
{
    //
    fprintf(stdout,"Looks like a Seg Fault tried to Occur(%d)\n", sig);
    exit(0);
}
void as_snd2_error()
{
    // If we're here.. a fatal error occurred.
    
}
