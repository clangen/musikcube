#ifndef ESD_H
#define ESD_H
#include <audiofile.h>

#ifdef __cplusplus
extern "C" {
#endif

/* length of the audio buffer size */
#define ESD_BUF_SIZE (4 * 1024)

/* length of the authorization key, octets */
#define ESD_KEY_LEN (16)

/* default port for the EsounD server */
#define ESD_DEFAULT_PORT (5001)

/* default sample rate for the EsounD server */
#define ESD_DEFAULT_RATE (44100)

/* maximum length of a stream/sample name */
#define ESD_NAME_MAX (128)

/* a magic number to identify the relative endianness of a client */
#define ESD_ENDIAN_KEY \
	( (unsigned int) ( ('E' << 24) + ('N' << 16) + ('D' << 8) + ('N') ) )

#define ESD_VOLUME_BASE (256)

/*************************************/
/* what can we do to/with the EsounD */
enum esd_proto { 
    ESD_PROTO_CONNECT,      /* implied on inital client connection */

    /* pseudo "security" functionality */
    ESD_PROTO_LOCK,	    /* disable "foreign" client connections */
    ESD_PROTO_UNLOCK,	    /* enable "foreign" client connections */

    /* stream functionality: play, record, monitor */
    ESD_PROTO_STREAM_PLAY,  /* play all following data as a stream */
    ESD_PROTO_STREAM_REC,   /* record data from card as a stream */
    ESD_PROTO_STREAM_MON,   /* send mixed buffer output as a stream */

    /* sample functionality: cache, free, play, loop, EOL, kill */
    ESD_PROTO_SAMPLE_CACHE, /* cache a sample in the server */
    ESD_PROTO_SAMPLE_FREE,  /* release a sample in the server */
    ESD_PROTO_SAMPLE_PLAY,  /* play a cached sample */
    ESD_PROTO_SAMPLE_LOOP,  /* loop a cached sample, til eoloop */
    ESD_PROTO_SAMPLE_STOP,  /* stop a looping sample when done */
    ESD_PROTO_SAMPLE_KILL,  /* stop the looping sample immed. */

    /* free and reclaim /dev/dsp functionality */
    ESD_PROTO_STANDBY,	    /* release /dev/dsp and ignore all data */
    ESD_PROTO_RESUME,	    /* reclaim /dev/dsp and play sounds again */

    /* TODO: move these to a more logical place. NOTE: will break the protocol */
    ESD_PROTO_SAMPLE_GETID, /* get the ID for an already-cached sample */
    ESD_PROTO_STREAM_FILT,  /* filter mixed buffer output as a stream */

    /* esd remote management */
    ESD_PROTO_SERVER_INFO,  /* get server info (ver, sample rate, format) */
    ESD_PROTO_ALL_INFO,     /* get all info (server info, players, samples) */
    ESD_PROTO_SUBSCRIBE,    /* track new and removed players and samples */
    ESD_PROTO_UNSUBSCRIBE,  /* stop tracking updates */

    /* esd remote control */
    ESD_PROTO_STREAM_PAN,   /* set stream panning */
    ESD_PROTO_SAMPLE_PAN,   /* set default sample panning */

    /* esd status */
    ESD_PROTO_STANDBY_MODE, /* see if server is in standby, autostandby, etc */

    ESD_PROTO_MAX           /* for bounds checking */
};
    

/******************/
/* The EsounD api */

/* the properties of a sound buffer are logically or'd */

/* bits of stream/sample data */
#define ESD_MASK_BITS	( 0x000F )
#define ESD_BITS8 	( 0x0000 )
#define ESD_BITS16	( 0x0001 )

/* how many interleaved channels of data */
#define ESD_MASK_CHAN	( 0x00F0 )
#define ESD_MONO	( 0x0010 )
#define ESD_STEREO	( 0x0020 )

/* whether it's a stream or a sample */
#define ESD_MASK_MODE	( 0x0F00 )
#define ESD_STREAM	( 0x0000 )
#define ESD_SAMPLE	( 0x0100 )
#define ESD_ADPCM	( 0x0200 )	/* TODO: anyone up for this? =P */

/* the function of the stream/sample, and common functions */
#define ESD_MASK_FUNC	( 0xF000 )
#define ESD_PLAY	( 0x1000 )
/* functions for streams only */
#define ESD_MONITOR	( 0x0000 )
#define ESD_RECORD	( 0x2000 )
/* functions for samples only */
#define ESD_STOP	( 0x0000 )
#define ESD_LOOP	( 0x2000 )

typedef int esd_format_t;
typedef int esd_proto_t;

/*******************************************************************/
/* client side API for playing sounds */

typedef unsigned char octet;

/*******************************************************************/
/* esdlib.c - basic esd client interface functions */

/* opens channel, authenticates connection, and prefares for protos */
/* returns EsounD socket for communication, result < 0 = error */
/* server = listen socket (localhost:5001, 192.168.168.0:9999 */
/* rate, format = (bits | channels | stream | func) */
int esd_open_sound( const char *host );

/* send the authorization cookie, create one if needed */
int esd_send_auth( int sock );

/* lock/unlock will disable/enable foreign clients from connecting */
int esd_lock( int esd );
int esd_unlock( int esd );

/* standby/resume will free/reclaim audio device so others may use it */
int esd_standby( int esd );
int esd_resume( int esd );

/* open a socket for playing, monitoring, or recording as a stream */
/* the *_fallback functions try to open /dev/dsp if there's no EsounD */
int esd_play_stream( esd_format_t format, int rate, 
		     const char *host, const char *name );
int esd_play_stream_fallback( esd_format_t format, int rate, 
			      const char *host, const char *name );
int esd_monitor_stream( esd_format_t format, int rate, 
			const char *host, const char *name );
/* int esd_monitor_stream_fallback( esd_format_t format, int rate ); */
int esd_record_stream( esd_format_t format, int rate, 
		       const char *host, const char *name );
int esd_record_stream_fallback( esd_format_t format, int rate, 
				const char *host, const char *name );
int esd_filter_stream( esd_format_t format, int rate, 
		       const char *host, const char *name );

/* cache a sample in the server returns sample id, < 0 = error */
int esd_sample_cache( int esd, esd_format_t format, int rate, 
		      int length, const char *name );
int esd_confirm_sample_cache( int esd );

/* get the sample id for an already-cached sample */
int esd_sample_getid( int esd, const char *name);

/* uncache a sample in the server */
int esd_sample_free( int esd, int sample );

/* play a cached sample once */
int esd_sample_play( int esd, int sample );
/* make a cached sample loop */
int esd_sample_loop( int esd, int sample );

/* stop the looping sample at end */
int esd_sample_stop( int esd, int sample );
/* stop a playing sample immed. */
int esd_sample_kill( int esd, int sample );

/* closes fd, previously obtained by esd_open */
int esd_close( int esd );


/*******************************************************************/
/* esdmgr.c - functions to implement a "sound manager" for esd */

/* structures to retrieve information about streams/samples from the server */
typedef struct esd_server_info {

    int version; 		/* server version encoded as an int */
    esd_format_t format;	/* magic int with the format info */
    int rate;			/* sample rate */

} esd_server_info_t;

typedef struct esd_player_info {

    struct esd_player_info *next; /* point to next entry in list */
    esd_server_info_t *server;	/* the server that contains this stream */
    
    int source_id;		/* either a stream fd or sample id */
    char name[ ESD_NAME_MAX ];	/* name of stream for remote control */
    int rate;			/* sample rate */
    int left_vol_scale;		/* volume scaling */
    int right_vol_scale;

    esd_format_t format;	/* magic int with the format info */

} esd_player_info_t;

typedef struct esd_sample_info {

    struct esd_sample_info *next; /* point to next entry in list */
    esd_server_info_t *server;	/* the server that contains this sample */
    
    int sample_id;		/* either a stream fd or sample id */
    char name[ ESD_NAME_MAX ];	/* name of stream for remote control */
    int rate;			/* sample rate */
    int left_vol_scale;		/* volume scaling */
    int right_vol_scale;

    esd_format_t format;	/* magic int with the format info */
    int length;			/* total buffer length */

} esd_sample_info_t;

typedef struct esd_info {

    esd_server_info_t *server;
    esd_player_info_t *player_list;
    esd_sample_info_t *sample_list;

} esd_info_t;

enum esd_standby_mode { 
    ESM_ERROR, ESM_ON_STANDBY, ESM_ON_AUTOSTANDBY, ESM_RUNNING
};
typedef int esd_standby_mode_t;

/* define callbacks for esd_update_info() */
/* what to do when a stream connects, or sample is played */
typedef int esd_new_player_callback_t( esd_player_info_t * );
/* what to do when a stream disconnects, or sample stops playing */
typedef int esd_old_player_callback_t( esd_player_info_t * );
/* what to do when a sample is cached */
typedef int esd_new_sample_callback_t( esd_sample_info_t * );
/* what to do when a sample is uncached */
typedef int esd_old_sample_callback_t( esd_sample_info_t * );

typedef struct esd_update_info_callbacks {
    esd_new_player_callback_t *esd_new_player_callback;
    esd_old_player_callback_t *esd_old_player_callback;
    esd_new_sample_callback_t *esd_new_sample_callback;
    esd_old_sample_callback_t *esd_old_sample_callback;
} esd_update_info_callbacks_t;

/* print server into to stdout */
void esd_print_server_info( esd_server_info_t *server_info );
void esd_print_player_info( esd_player_info_t *player_info );
void esd_print_sample_info( esd_sample_info_t *sample_info );
/* print all info to stdout */
void esd_print_all_info( esd_info_t *all_info );

/* retrieve server properties (sample rate, format, version number) */
esd_server_info_t *esd_get_server_info( int esd );
/* release all memory allocated for the server properties structure */
void esd_free_server_info( esd_server_info_t *server_info );

/* retrieve all information from server */
esd_info_t *esd_get_all_info( int esd );

/* retrieve all information from server, and update until unsubsribed or closed */
esd_info_t *esd_subscribe_all_info( int esd );

/* call to update the info structure with new information, and call callbacks */
esd_info_t *esd_update_info( int esd, esd_info_t *info, 
			     esd_update_info_callbacks_t *callbacks );
esd_info_t *esd_unsubscribe_info( int esd );

/* release all memory allocated for the esd info structure */
void esd_free_all_info( esd_info_t *info );


/* reset the volume panning for a stream */
int esd_set_stream_pan( int esd, int stream_id, 
			int left_scale, int right_scale );

/* reset the default volume panning for a sample */
int esd_set_default_sample_pan( int esd, int sample_id, 
				int left_scale, int right_scale );

/* see if the server is in stnaby, autostandby, etc */
esd_standby_mode_t esd_get_standby_mode( int esd );


/*******************************************************************/
/* esdfile.c - audiofile wrappers for sane handling of files */

int esd_send_file( int esd, AFfilehandle au_file, int frame_length );
int esd_play_file( const char *name_prefix, const char *filename, int fallback );
int esd_file_cache( int esd, const char *name_prefix, const char *filename );


/*******************************************************************/
/* audio.c - abstract the sound hardware for cross platform usage */
extern esd_format_t esd_audio_format;
extern int esd_audio_rate;
extern char *esd_audio_device;

const char *esd_audio_devices( void );
int esd_audio_open( void );
void esd_audio_close( void );
void esd_audio_pause( void );
int esd_audio_write( void *buffer, int buf_size );
int esd_audio_read( void *buffer, int buf_size );
void esd_audio_flush( void );

#ifdef __cplusplus
}
#endif


#endif /* #ifndef ESD_H */
