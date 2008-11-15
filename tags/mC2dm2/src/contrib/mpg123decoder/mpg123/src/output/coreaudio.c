/*
	coreaudio: audio output on MacOS X

	copyright ?-2006 by the mpg123 project - free software under the terms of the GPL 2
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Guillaume Outters
	modified by Nicholas J Humfrey to use SFIFO code
	modified by Taihei Monma to use AudioUnit and AudioConverter APIs
*/


#include "sfifo.h"
#include "mpg123app.h"

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <errno.h>
#include "debug.h"

#define FIFO_DURATION		(0.5f)		/* Duration of the ring buffer in seconds */


typedef struct mpg123_coreaudio
{
	AudioConverterRef converter;
	AudioUnit outputUnit;
	int open;
	char play;
	int channels;
	int last_buffer;
	int play_done;
	int decode_done;
	
	/* Convertion buffer */
	unsigned char * buffer;
	size_t buffer_size;
	
	/* Ring buffer */
	sfifo_t fifo;

} mpg123_coreaudio_t;



static OSStatus playProc(AudioConverterRef inAudioConverter,
						 UInt32 *ioNumberDataPackets,
                         AudioBufferList *outOutputData,
                         AudioStreamPacketDescription **outDataPacketDescription,
                         void* inClientData)
{
	mpg123_coreaudio_t *ca = (mpg123_coreaudio_t *)inClientData;
	long n;
	
	
	if(ca->last_buffer) {
		ca->play_done = 1;
		return noErr;
	}
	
	for(n = 0; n < outOutputData->mNumberBuffers; n++)
	{
		unsigned int wanted = *ioNumberDataPackets * ca->channels * 2;
		unsigned char *dest;
		unsigned int read;
		if(ca->buffer_size < wanted) {
			debug1("Allocating %d byte sample conversion buffer", wanted);
			ca->buffer = realloc( ca->buffer, wanted);
			ca->buffer_size = wanted;
		}
		dest = ca->buffer;
		
		/* Only play if we have data left */
		if ( sfifo_used( &ca->fifo ) < wanted ) {
			if(!ca->decode_done) {
				warning("Didn't have any audio data in callback (buffer underflow)");
				return -1;
			}
			wanted = sfifo_used( &ca->fifo );
			ca->last_buffer = 1;
		}
		
		/* Read audio from FIFO to SDL's buffer */
		read = sfifo_read( &ca->fifo, dest, wanted );
		
		if (wanted!=read)
			warning2("Error reading from the ring buffer (wanted=%u, read=%u).\n", wanted, read);
		
		outOutputData->mBuffers[n].mDataByteSize = read;
		outOutputData->mBuffers[n].mData = dest;
	}
	
	return noErr; 
}

static OSStatus convertProc(void *inRefCon, AudioUnitRenderActionFlags *inActionFlags,
                            const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                            UInt32 inNumFrames, AudioBufferList *ioData)
{
	AudioStreamPacketDescription* outPacketDescription = NULL;
	mpg123_coreaudio_t* ca = (mpg123_coreaudio_t*)inRefCon;
	OSStatus err= noErr;
	
	err = AudioConverterFillComplexBuffer(ca->converter, playProc, inRefCon, &inNumFrames, ioData, outPacketDescription);
	
	return err;
}

static int open_coreaudio(audio_output_t *ao)
{
	mpg123_coreaudio_t* ca = (mpg123_coreaudio_t*)ao->userptr;
	UInt32 size;
	ComponentDescription desc;
	Component comp;
	AudioStreamBasicDescription inFormat;
	AudioStreamBasicDescription outFormat;
	AURenderCallbackStruct  renderCallback;
	Boolean outWritable;
	
	/* Initialize our environment */
	ca->play = 0;
	ca->buffer = NULL;
	ca->buffer_size = 0;
	ca->last_buffer = 0;
	ca->play_done = 0;
	ca->decode_done = 0;

	
	/* Get the default audio output unit */
	desc.componentType = kAudioUnitType_Output; 
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	comp = FindNextComponent(NULL, &desc);
	if(comp == NULL) {
		error("FindNextComponent failed");
		return(-1);
	}
	
	if(OpenAComponent(comp, &(ca->outputUnit)))  {
		error("OpenAComponent failed");
		return (-1);
	}
	
	if(AudioUnitInitialize(ca->outputUnit)) {
		error("AudioUnitInitialize failed");
		return (-1);
	}
	
	/* Specify the output PCM format */
	AudioUnitGetPropertyInfo(ca->outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &size, &outWritable);
	if(AudioUnitGetProperty(ca->outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outFormat, &size)) {
		error("AudioUnitGetProperty(kAudioUnitProperty_StreamFormat) failed");
		return (-1);
	}
	
	if(AudioUnitSetProperty(ca->outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &outFormat, size)) {
		error("AudioUnitSetProperty(kAudioUnitProperty_StreamFormat) failed");
		return (-1);
	}
	
	/* Specify the input PCM format */
	ca->channels = ao->channels;
	inFormat.mSampleRate = ao->rate;
	inFormat.mChannelsPerFrame = ao->channels;
	inFormat.mBitsPerChannel = 16;
	inFormat.mBytesPerPacket = 2*inFormat.mChannelsPerFrame;
	inFormat.mFramesPerPacket = 1;
	inFormat.mBytesPerFrame = 2*inFormat.mChannelsPerFrame;
	inFormat.mFormatID = kAudioFormatLinearPCM;
#ifdef _BIG_ENDIAN
	inFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked | kLinearPCMFormatFlagIsBigEndian;
#else
	inFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
#endif
	
		
	/* Add our callback - but don't start it yet */
	memset(&renderCallback, 0, sizeof(AURenderCallbackStruct));
	renderCallback.inputProc = convertProc;
	renderCallback.inputProcRefCon = ao->userptr;
	if(AudioUnitSetProperty(ca->outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &renderCallback, sizeof(AURenderCallbackStruct))) {
		error("AudioUnitSetProperty(kAudioUnitProperty_SetRenderCallback) failed");
		return(-1);
	}
	
	
	/* Open an audio I/O stream and create converter */
	if (ao->rate > 0 && ao->channels >0 ) {
		int ringbuffer_len;

		if(AudioConverterNew(&inFormat, &outFormat, &(ca->converter))) {
			error("AudioConverterNew failed");
			return(-1);
		}
		if(ao->channels == 1) {
			SInt32 channelMap[2] = { 0, 0 };
			if(AudioConverterSetProperty(ca->converter, kAudioConverterChannelMap, sizeof(channelMap), channelMap)) {
				error("AudioConverterSetProperty(kAudioConverterChannelMap) failed");
				return(-1);
			}
		}
		
		/* Initialise FIFO */
		ringbuffer_len = ao->rate * FIFO_DURATION * sizeof(short) *ao->channels;
		debug2( "Allocating %d byte ring-buffer (%f seconds)", ringbuffer_len, (float)FIFO_DURATION);
		sfifo_init( &ca->fifo, ringbuffer_len );
									   
	}
	
	return(0);
}

static int get_formats_coreaudio(audio_output_t *ao)
{
	/* Only support Signed 16-bit output */
	return MPG123_ENC_SIGNED_16;
}

static int write_coreaudio(audio_output_t *ao, unsigned char *buf, int len)
{
	mpg123_coreaudio_t* ca = (mpg123_coreaudio_t*)ao->userptr;
	int written;

	/* If there is no room, then sleep for half the length of the FIFO */
	while (sfifo_space( &ca->fifo ) < len ) {
		usleep( (FIFO_DURATION/2) * 1000000 );
	}
	
	/* Store converted audio in ring buffer */
	written = sfifo_write( &ca->fifo, (char*)buf, len);
	if (written != len) {
		warning( "Failed to write audio to ring buffer" );
		return -1;
	}
	
	/* Start playback now that we have something to play */
	if(!ca->play)
	{
		if(AudioOutputUnitStart(ca->outputUnit)) {
			error("AudioOutputUnitStart failed");
			return(-1);
		}
		ca->play = 1;
	}
	
	return len;
}

static int close_coreaudio(audio_output_t *ao)
{
	mpg123_coreaudio_t* ca = (mpg123_coreaudio_t*)ao->userptr;

	if (ca) {
		ca->decode_done = 1;
		while(!ca->play_done && ca->play) usleep(10000);
		
		/* No matter the error code, we want to close it (by brute force if necessary) */
		AudioConverterDispose(ca->converter);
		AudioOutputUnitStop(ca->outputUnit);
		AudioUnitUninitialize(ca->outputUnit);
		CloseComponent(ca->outputUnit);
	
	    /* Free the ring buffer */
		sfifo_close( &ca->fifo );
		
		/* Free the conversion buffer */
		if (ca->buffer) {
			free( ca->buffer );
			ca->buffer = NULL;
		}
		
	}
	
	return 0;
}

static void flush_coreaudio(audio_output_t *ao)
{
	mpg123_coreaudio_t* ca = (mpg123_coreaudio_t*)ao->userptr;

	/* Stop playback */
	if(AudioOutputUnitStop(ca->outputUnit)) {
		error("AudioOutputUnitStop failed");
	}
	ca->play=0;
	
	/* Empty out the ring buffer */
	sfifo_flush( &ca->fifo );	
}

static int deinit_coreaudio(audio_output_t* ao)
{
	/* Free up memory */
	if (ao->userptr) {
		free( ao->userptr );
		ao->userptr = NULL;
	}

	/* Success */
	return 0;
}

static int init_coreaudio(audio_output_t* ao)
{
	if (ao==NULL) return -1;

	/* Set callbacks */
	ao->open = open_coreaudio;
	ao->flush = flush_coreaudio;
	ao->write = write_coreaudio;
	ao->get_formats = get_formats_coreaudio;
	ao->close = close_coreaudio;
	ao->deinit = deinit_coreaudio;

	/* Allocate memory for data structure */
	ao->userptr = malloc( sizeof( mpg123_coreaudio_t ) );
	if (ao->userptr==NULL) {
		error("failed to malloc memory for 'mpg123_coreaudio_t'");
		return -1;
	}
	memset( ao->userptr, 0, sizeof(mpg123_coreaudio_t) );

	/* Success */
	return 0;
}



/* 
	Module information data structure
*/
mpg123_module_t mpg123_output_module_info = {
	/* api_version */	MPG123_MODULE_API_VERSION,
	/* name */			"coreaudio",						
	/* description */	"Output audio using Mac OS X's CoreAudio.",
	/* revision */		"$Rev:$",
	/* handle */		NULL,
	
	/* init_output */	init_coreaudio,						
};


