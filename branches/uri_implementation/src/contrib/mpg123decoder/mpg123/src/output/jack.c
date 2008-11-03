/*
	jack: audio output via JACK Audio Connection Kit

	copyright 2006 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Nicholas J. Humfrey
*/

#include <math.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "mpg123app.h"
#include "debug.h"

#define MAX_CHANNELS	(2)

typedef struct {
	int channels;
	jack_port_t *ports[MAX_CHANNELS];
	jack_ringbuffer_t * rb[MAX_CHANNELS];
	size_t rb_size;
	jack_client_t *client;
	jack_default_audio_sample_t *tmp_buffer;
} jack_handle_t, *jack_handle_ptr;


static jack_handle_t* alloc_jack_handle()
{
	jack_handle_t *handle=NULL;

	handle = malloc(sizeof(jack_handle_t));
	if (!handle) {
		error("Failed to allocate memory for our handle.");
		return NULL;
	}

	/* Initialise the handle, and store for later*/
	handle->rb[0] = NULL;
	handle->rb[1] = NULL;
	handle->ports[0] = NULL;
	handle->ports[1] = NULL;
	handle->client = NULL;
	handle->tmp_buffer = NULL;
	handle->rb_size = 0;
	
	return handle;
}


static void free_jack_handle( jack_handle_t* handle )
{
	int i;
	
	for(i=0; i<MAX_CHANNELS; i++) {
		/* Close the port for channel*/
		if ( handle->ports[i] )
			jack_port_unregister( handle->client, handle->ports[i] );

		/* Free up the ring buffer for channel*/
		if ( handle->rb[i] )
			jack_ringbuffer_free( handle->rb[i] );
	}

	if (handle->client)
		jack_client_close(handle->client);
		
	if (handle->tmp_buffer)
		free(handle->tmp_buffer);

	free(handle);
}


static int process_callback( jack_nframes_t nframes, void *arg )
{
	jack_handle_t* handle = (jack_handle_t*)arg;
    size_t to_read = sizeof (jack_default_audio_sample_t) * nframes;
	unsigned int c;
	

    /* copy data to ringbuffer; one per channel*/
    for (c=0; c < handle->channels; c++)
    {	
		char *buf = (char*)jack_port_get_buffer(handle->ports[c], nframes);
		size_t len = jack_ringbuffer_read(handle->rb[c], buf, to_read);
		
		/* If we don't have enough audio, fill it up with silence*/
		/* (this is to deal with pausing etc.)*/
		if (to_read > len)
			bzero( buf+len, to_read - len );
		
		/*if (len < to_read)*/
		/*	error1("failed to read from ring buffer %d",c);*/
    }

	/* Success*/
	return 0;
}

static void shutdown_callback( void *arg )
{
/*	jack_handle_t* handle = (jack_handle_t*)arg; */

	debug("shutdown_callback()");

}

/* crude way of automatically connecting up jack ports */
/* 0 on error */
static int autoconnect_jack_ports( jack_handle_t* handle )
{
	const char **all_ports;
	unsigned int ch=0;
	int err,i;

	/* Get a list of all the jack ports*/
	all_ports = jack_get_ports (handle->client, NULL, NULL, JackPortIsInput);
	if (!all_ports) {
		error("connect_jack_ports(): jack_get_ports() returned NULL.");
		return 0;
	}
	
	/* Step through each port name*/
	for (i = 0; all_ports[i]; ++i) {

		const char* in = jack_port_name( handle->ports[ch] );
		const char* out = all_ports[i];
		
		debug2("Connecting %s to %sesound", in, out);
		
		if ((err = jack_connect(handle->client, in, out)) != 0) {
			error1("connect_jack_ports(): failed to jack_connect() ports: %d",err);
			return 0;
		}
	
		/* Found enough ports ?*/
		if (++ch >= handle->channels) break;
	}
	
	free( all_ports );
	return 1;
}


static int connect_jack_ports( jack_handle_t* handle, const char *dev ) 
{
	if (dev==NULL || strcmp(dev, "auto")==0) {
		return autoconnect_jack_ports( handle );
	} else if (strcmp(dev, "none")==0) {
		warning("Not connecting up jack ports as requested.");
	} else {
		warning("Sorry I don't know how to connect up ports yet.");
	}
	return 1;
}


static int close_jack(audio_output_t *ao)
{
	jack_handle_t *handle = (jack_handle_t*)ao->userptr;
	
	debug("close_jack().");

	/* Close and shutdown*/
	if (handle) {
		free_jack_handle( handle );
		ao->userptr = NULL;
    }
    
	return 0;
}


static int open_jack(audio_output_t *ao)
{
	char client_name[255];
	jack_handle_t *handle=NULL;
	jack_options_t jopt = JackNullOption;
	jack_status_t jstat = 0;
	unsigned int i;

	debug("jack open");
	if(!ao) return -1;

	/* Return if already open*/
	if (ao->userptr) {
		error("audio device is already open.");
		return -1;
	}

	/* Create some storage for ourselves*/
	if((handle = alloc_jack_handle()) == NULL) return -1;

	ao->userptr = (void*)handle;

	/* Register with Jack*/
	snprintf(client_name, 255, "mpg123-%d", getpid());
	if ((handle->client = jack_client_open(client_name, jopt, &jstat)) == 0) {
		error1("Failed to open jack client: 0x%x", jstat);
		close_jack(ao);
		return -1;
	}
	
	/* Display the unique client name allocated to us */
	fprintf(stderr,"Registered as JACK client %s.\n",
		jack_get_client_name( handle->client ) );

	/* The initial open lets me choose the settings. */
	if (ao->format==-1)
	{
		ao->format = MPG123_ENC_16;
		ao->rate   = jack_get_sample_rate(handle->client);
		ao->channels = 2;
	}

	/* Check the sample rate is correct*/
	if (jack_get_sample_rate( handle->client ) != (jack_nframes_t)ao->rate) {
		error("JACK Sample Rate is different to sample rate of file.");
		close_jack(ao);
		return -1;
	}

	/* Register ports with Jack*/
	handle->channels = ao->channels;
	if (handle->channels == 1) {
		if (!(handle->ports[0] = jack_port_register(handle->client, "mono", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)))
		{
			error("Cannot register JACK output port 'mono'.");
			close_jack(ao);
			return -1;
		}
	} else if (handle->channels == 2) {
		if (!(handle->ports[0] = jack_port_register(handle->client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)))
		{
			error("Cannot register JACK output port 'left'.");
			close_jack(ao);
			return -1;
		}
		if (!(handle->ports[1] = jack_port_register(handle->client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)))
		{
			error("Cannot register JACK output port 'right'.");
			close_jack(ao);
			return -1;
		}
	} else {
		error1("invalid number of output channels (%d).", handle->channels);
		close_jack(ao);
		return -1;
	}

	/* Create the ring buffers (one seconds audio)*/
    handle->rb_size = jack_get_sample_rate(handle->client) * sizeof(jack_default_audio_sample_t);
    for(i=0;i<handle->channels;i++){
        handle->rb[i]=jack_ringbuffer_create(handle->rb_size);
    }

	/* Set the callbacks*/
	jack_set_process_callback(handle->client, process_callback, (void*)handle);
	jack_on_shutdown(handle->client, shutdown_callback, (void*)handle);
	
	/* Activate client*/
	if (jack_activate(handle->client)) {
		error("Can't activate client.");
	}

	/* Connect up the portsm, return */
	if(!connect_jack_ports( handle, ao->device ))
	{
		/* deregistering of ports will not work but should just fail, then, and let the rest clean up */
		close_jack(ao);
		return -1;
	}

	debug("Jack open successful.\n");
	return 0;
}


/* Jack could be set to 32bits float, actually */
static int get_formats_jack(audio_output_t *ao)
{
	if(jack_get_sample_rate( ((jack_handle_t*)(ao->userptr))->client ) != (jack_nframes_t)ao->rate) return 0;
	else return MPG123_ENC_SIGNED_16;
}


static int write_jack(audio_output_t *ao, unsigned char *buf, int len)
{
	int c,n = 0;
	short* src = (short*)buf;
	jack_handle_t *handle = (jack_handle_t*)ao->userptr;
	jack_nframes_t samples = len / 2 / handle->channels;
	size_t tmp_size = samples * sizeof( jack_default_audio_sample_t );
	
	
	/* Sanity check that ring buffer is at least twice the size of the audio we just got*/
	if (handle->rb_size/2 < len) {
		error("ring buffer is less than twice the size of audio given.");
		return -1;
	}
	
	
	/* Wait until there is space in the ring buffer*/
	while (jack_ringbuffer_write_space( handle->rb[0] ) < tmp_size) {
		/* Sleep for a quarter of the ring buffer size (1/4 second)*/
		usleep(250000);
	}
	
	
	/* Ensure the temporary buffer is big enough*/
	handle->tmp_buffer = (jack_default_audio_sample_t*)realloc( handle->tmp_buffer, tmp_size);
	if (!handle->tmp_buffer) {
		error("failed to realloc temporary buffer.");
		return -1;
	}
	
	
	for(c=0; c<handle->channels; c++) {
		size_t len = 0;
		
		/* Convert samples from short to flat and put in temporary buffer*/
		for(n=0; n<samples; n++) {
			handle->tmp_buffer[n] = src[(n*handle->channels)+c] / 32768.0f;
		}
		
		/* Copy temporary buffer into ring buffer*/
		len = jack_ringbuffer_write(handle->rb[c], (char*)handle->tmp_buffer, tmp_size);
		if (len < tmp_size)
        {
			error("failed to write to ring ruffer.");
			return -1;
		}
	}
	
	
	return len;
}

static void flush_jack(audio_output_t *ao)
{
	jack_handle_t *handle = (jack_handle_t*)ao->userptr;
	int c;

	/* Reset the ring buffers*/
	for(c=0; c<handle->channels; c++) {
		jack_ringbuffer_reset(handle->rb[c]);
	}
}

static int init_jack(audio_output_t* ao)
{
	if (ao==NULL) return -1;
	
	/* Set callbacks */
	ao->open = open_jack;
	ao->flush = flush_jack;
	ao->write = write_jack;
	ao->get_formats = get_formats_jack;
	ao->close = close_jack;

	/* Success */
	return 0;
}


/* 
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"jack",
	/* description */	"Output audio using JACK (JACK Audio Connection Kit).",
	/* revision */		"$Rev:$",
	/* handle */		NULL,

	/* init_output */	init_jack,						
};
