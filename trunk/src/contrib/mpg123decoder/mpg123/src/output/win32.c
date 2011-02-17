/*
	win32: audio output for Windows 32bit

	copyright ?-2007 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org

	initially written (as it seems) by Tony Million
	rewrite of basic functionality for callback-less and properly ringbuffered operation by ravenexp
*/

#include "mpg123app.h"
#include <windows.h>
#include "debug.h"

/*
	Buffer size and number of buffers in the playback ring
	NOTE: This particular num/size combination performs best under heavy
	loads for my system, however this may not be true for any hardware/OS out there.
	Generally, BUFFER_SIZE < 8k || NUM_BUFFERS > 16 || NUM_BUFFERS < 4 are not recommended.
*/
#define BUFFER_SIZE 0x10000
#define NUM_BUFFERS 8  /* total 512k roughly 2.5 sec of CD quality sound */

/* Buffer ring queue state */
struct queue_state
{
	WAVEHDR buffer_headers[NUM_BUFFERS];
	/* The next buffer to be filled and put in playback */
	int next_buffer;
	/* Buffer playback completion event */
	HANDLE play_done_event;
	HWAVEOUT waveout;
};

static int open_win32(struct audio_output_struct *ao)
{
	struct queue_state* state;
	int i;
	MMRESULT res;
	WAVEFORMATEX out_fmt;
	UINT dev_id;

	if(!ao) return -1;
	if(ao->rate == -1) return 0;

	/* Allocate queue state struct for this device */
	state = calloc(1, sizeof(struct queue_state));
	if(!state) return -1;

	ao->userptr = state;
	/* Allocate playback buffers */
	for(i = 0; i < NUM_BUFFERS; i++)
	if(!(state->buffer_headers[i].lpData = malloc(BUFFER_SIZE)))
	ereturn(-1, "Out of memory for playback buffers.");

	state->play_done_event = CreateEvent(0,FALSE,FALSE,0);
	if(state->play_done_event == INVALID_HANDLE_VALUE) return -1;

	/* FIXME: real device enumeration by capabilities? */
	dev_id = WAVE_MAPPER;	/* probably does the same thing */
	ao->device = "WaveMapper";
	/* FIXME: support for smth besides MPG123_ENC_SIGNED_16? */
	out_fmt.wFormatTag = WAVE_FORMAT_PCM;
	out_fmt.wBitsPerSample = 16;
	out_fmt.nChannels = ao->channels;
	out_fmt.nSamplesPerSec = ao->rate;
	out_fmt.nBlockAlign = out_fmt.nChannels*out_fmt.wBitsPerSample/8;
	out_fmt.nAvgBytesPerSec = out_fmt.nBlockAlign*out_fmt.nSamplesPerSec;
	out_fmt.cbSize = 0;

	res = waveOutOpen(&state->waveout, dev_id, &out_fmt,
	                  (DWORD_PTR)state->play_done_event, 0, CALLBACK_EVENT);

	switch(res)
	{
		case MMSYSERR_NOERROR:
			break;
		case MMSYSERR_ALLOCATED:
			ereturn(-1, "Audio output device is already allocated.");
		case MMSYSERR_NODRIVER:
			ereturn(-1, "No device driver is present.");
		case MMSYSERR_NOMEM:
			ereturn(-1, "Unable to allocate or lock memory.");
		case WAVERR_BADFORMAT:
			ereturn(-1, "Unsupported waveform-audio format.");
		default:
			ereturn(-1, "Unable to open wave output device.");
	}

	/* Reset event from the "device open" message */
	ResetEvent(state->play_done_event);
	return 0;
}

static int get_formats_win32(struct audio_output_struct *ao)
{
	/* FIXME: support for smth besides MPG123_ENC_SIGNED_16? */
	return MPG123_ENC_SIGNED_16;
}

/* Stores audio data to the fixed size buffers and pushes them into the playback queue.
   I have one grief with that: The last piece of a track may not reach the output,
   only full buffers sent... But we don't get smooth audio otherwise. */
static int write_win32(struct audio_output_struct *ao, unsigned char *buf, int len)
{
	struct queue_state* state;
	MMRESULT res;
	WAVEHDR* hdr;

	int rest_len; /* Input data bytes left for next recursion. */
	int bufill;   /* Bytes we stuff into buffer now. */

	if(!ao || !ao->userptr) return -1;
	if(!buf || len <= 0) return 0;

	state = (struct queue_state*)ao->userptr;
	hdr = &state->buffer_headers[state->next_buffer];

	/* Check buffer header and wait if it's being played.
	   Skip waiting if the buffer is not full yet */
	while(hdr->dwBufferLength == BUFFER_SIZE && !(hdr->dwFlags & WHDR_DONE))
	{
		/* debug1("waiting for buffer %i...", state->next_buffer); */
		WaitForSingleObject(state->play_done_event, INFINITE);
	}

	/* If it was a full buffer being played, clean up. */
	if(hdr->dwFlags & WHDR_DONE)
	{
		waveOutUnprepareHeader(state->waveout, hdr, sizeof(WAVEHDR));
		hdr->dwFlags = 0;
		hdr->dwBufferLength = 0;
	}

	/* Now see how much we want to stuff in and then stuff it in. */
	bufill = BUFFER_SIZE - hdr->dwBufferLength;
	if(len < bufill) bufill = len;

	rest_len = len - bufill;
	memcpy(hdr->lpData + hdr->dwBufferLength, buf, bufill);
	hdr->dwBufferLength += bufill;
	if(hdr->dwBufferLength == BUFFER_SIZE)
	{ /* Send the buffer out when it's full. */
		res = waveOutPrepareHeader(state->waveout, hdr, sizeof(WAVEHDR));
		if(res != MMSYSERR_NOERROR) ereturn(-1, "Can't write to audio output device (prepare).");

		res = waveOutWrite(state->waveout, hdr, sizeof(WAVEHDR));
		if(res != MMSYSERR_NOERROR) ereturn(-1, "Can't write to audio output device.");

		/* Cycle to the next buffer in the ring queue */
		state->next_buffer = (state->next_buffer + 1) % NUM_BUFFERS;
	}
	/* I'd like to propagate error codes or something... but there are no catchable surprises left.
	   Anyhow: Here is the recursion that makes ravenexp happy;-) */
	if(rest_len && write_win32(ao, buf + bufill, rest_len) < 0) /* Write the rest. */
	return -1;
	else    
	return len;
}

static void flush_win32(struct audio_output_struct *ao)
{
	int i, z;
	struct queue_state* state;

	if(!ao || !ao->userptr) return;
	state = (struct queue_state*)ao->userptr;

	/* FIXME: The very last output buffer is not played. This could be a problem on the feeding side. */
	i = 0;
	z = state->next_buffer - 1;
	for(i = 0; i < NUM_BUFFERS; i++)
	{
		if(!state->buffer_headers[i].dwFlags & WHDR_DONE)
			WaitForSingleObject(state->play_done_event, INFINITE);

		waveOutUnprepareHeader(state->waveout, &state->buffer_headers[i], sizeof(WAVEHDR));
		state->buffer_headers[i].dwFlags = 0;
		state->buffer_headers[i].dwBufferLength = 0;
		z = (z + 1) % NUM_BUFFERS;
	}
}

static int close_win32(struct audio_output_struct *ao)
{
	int i;
	struct queue_state* state;

	if(!ao || !ao->userptr) return -1;
	state = (struct queue_state*)ao->userptr;

	flush_win32(ao);
	waveOutClose(state->waveout);
	CloseHandle(state->play_done_event);

	for(i = 0; i < NUM_BUFFERS; i++) free(state->buffer_headers[i].lpData);

	free(ao->userptr);
	ao->userptr = 0;
	return 0;
}

static int init_win32(audio_output_t* ao)
{
	if(!ao) return -1;

	/* Set callbacks */
	ao->open = open_win32;
	ao->flush = flush_win32;
	ao->write = write_win32;
	ao->get_formats = get_formats_win32;
	ao->close = close_win32;

	/* Success */
	return 0;
}

/* 
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"win32",						
	/* description */	"Audio output for Windows (winmm).",
	/* revision */		"$Rev:$",						
	/* handle */		NULL,
	
	/* init_output */	init_win32,						
};
