/*
	portaudio: audio output via PortAudio cross-platform audio API

	copyright 2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Nicholas J. Humfrey
*/

#include <math.h>

#include <portaudio.h>

#include "audio.h"
#include "sfifo.h"
#include "mpg123app.h"

#ifdef WIN32
#include <windows.h>
#endif
#include "debug.h"

#define SAMPLE_SIZE			(2)
#define FRAMES_PER_BUFFER	(256)
#define FIFO_DURATION		(0.5f)


typedef struct {
	PaStream *stream;
	sfifo_t fifo;
} mpg123_portaudio_t;

#ifdef PORTAUDIO18
#define PaTime PaTimestamp
#define Pa_IsStreamActive Pa_StreamActive
#endif


#ifdef PORTAUDIO18
static int paCallback( void *inputBuffer, void *outputBuffer,
			 unsigned long framesPerBuffer,
			 PaTime outTime, void *userData )
#else
static int paCallback(
    const void *inputBuffer, void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData )
#endif
{
	audio_output_t *ao = userData;
	mpg123_portaudio_t *pa = (mpg123_portaudio_t*)ao->userptr;
	unsigned long bytes = framesPerBuffer * SAMPLE_SIZE * ao->channels;
	
	if (sfifo_used(&pa->fifo)<bytes) {
		error("ringbuffer for PortAudio is empty");
		return 1;
	} else {
		sfifo_read( &pa->fifo, outputBuffer, bytes );
		return 0;
	}
}


static int open_portaudio(audio_output_t *ao)
{
	mpg123_portaudio_t *pa = (mpg123_portaudio_t*)ao->userptr;
	PaError err;
	
	/* Open an audio I/O stream. */
	if (ao->rate > 0 && ao->channels >0 ) {
	
		err = Pa_OpenDefaultStream(
					&pa->stream,
					0,          	/* no input channels */
					ao->channels,	/* number of output channels */
					paInt16,		/* signed 16-bit samples */
					ao->rate,		/* sample rate */
					FRAMES_PER_BUFFER,	/* frames per buffer */
#ifdef PORTAUDIO18
					0,				/* number of buffers, if zero then use default minimum */
#endif
					paCallback,		/* no callback - use blocking IO */
					ao );
			
		if( err != paNoError ) {
			error1("Failed to open PortAudio default stream: %s", Pa_GetErrorText( err ));
			return -1;
		}
		
		/* Initialise FIFO */
		sfifo_init( &pa->fifo, ao->rate * FIFO_DURATION * SAMPLE_SIZE *ao->channels );
									   
	}
	
	return(0);
}


static int get_formats_portaudio(audio_output_t *ao)
{
	/* Only implemented Signed 16-bit audio for now */
	return MPG123_ENC_SIGNED_16;
}


static int write_portaudio(audio_output_t *ao, unsigned char *buf, int len)
{
	mpg123_portaudio_t *pa = (mpg123_portaudio_t*)ao->userptr;
	PaError err;
	int written;
	
	/* Sleep for half the length of the FIFO */
	while (sfifo_space( &pa->fifo ) < len ) {
#ifdef WIN32
		Sleep( (FIFO_DURATION/2) * 1000);
#else
		usleep( (FIFO_DURATION/2) * 1000000 );
#endif
	}

	/* Write the audio to the ring buffer */
	written = sfifo_write( &pa->fifo, buf, len );

	/* Start stream if not ative */
	err = Pa_IsStreamActive( pa->stream );
	if (err == 0) {
		err = Pa_StartStream( pa->stream );
		if( err != paNoError ) {
			error1("Failed to start PortAudio stream: %s", Pa_GetErrorText( err ));
			return -1; /* triggering exit here is not good, better handle that somehow... */
		}
	} else if (err < 0) {
		error1("Failed to check state of PortAudio stream: %s", Pa_GetErrorText( err ));
		return -1;
	}
	
	return written;
}


static int close_portaudio(audio_output_t *ao)
{
	mpg123_portaudio_t *pa = (mpg123_portaudio_t*)ao->userptr;
	PaError err;
	
	if (pa->stream) {
		/* stop the stream if it is active */
		if (Pa_IsStreamActive( pa->stream ) == 1) {
			err = Pa_StopStream( pa->stream );
			if( err != paNoError ) {
				error1("Failed to stop PortAudio stream: %s", Pa_GetErrorText( err ));
				return -1;
			}
		}
	
		/* and then close the stream */
		err = Pa_CloseStream( pa->stream );
		if( err != paNoError ) {
			error1("Failed to close PortAudio stream: %s", Pa_GetErrorText( err ));
			return -1;
		}
		
		pa->stream = NULL;
	}
	
	/* and free memory used by fifo */
	sfifo_close( &pa->fifo );
    
	return 0;
}


static void flush_portaudio(audio_output_t *ao)
{
	mpg123_portaudio_t *pa = (mpg123_portaudio_t*)ao->userptr;
	PaError err;
	
	/* throw away contents of FIFO */
	sfifo_flush( &pa->fifo );
	
	/* and empty out PortAudio buffers */
	err = Pa_AbortStream( pa->stream );
	
}


static int deinit_portaudio(audio_output_t* ao)
{
	/* Free up memory */
	if (ao->userptr) {
		free( ao->userptr );
		ao->userptr = NULL;
	}

	/* Shut down PortAudio */
	Pa_Terminate();

	/* Success */
	return 0;
}


static int init_portaudio(audio_output_t* ao)
{
	int err = paNoError;
	
	if (ao==NULL) return -1;
	
	/* Set callbacks */
	ao->open = open_portaudio;
	ao->flush = flush_portaudio;
	ao->write = write_portaudio;
	ao->get_formats = get_formats_portaudio;
	ao->close = close_portaudio;
	ao->deinit = deinit_portaudio;

	/* Allocate memory for handle */
	ao->userptr = malloc( sizeof(mpg123_portaudio_t) );
	if (ao->userptr==NULL) {
		error( "Failed to allocated memory for driver structure" );
		return -1;
	}
	memset( ao->userptr, 0, sizeof(mpg123_portaudio_t) );

	/* Initialise PortAudio */
	err = Pa_Initialize();
	if( err != paNoError ) {
		error1("Failed to initialise PortAudio: %s", Pa_GetErrorText( err ));
		return -1;
	}

	/* Success */
	return 0;
}



/* 
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"portaudio",						
	/* description */	"Output audio using PortAudio",
	/* revision */		"$Rev:$",						
	/* handle */		NULL,
	
	/* init_output */	init_portaudio,						
};

