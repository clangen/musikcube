/*
	wav.c: write wav files

	copyright ?-2008 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Samuel Audet

	Geez, why are WAV RIFF headers are so secret?  I got something together,
	but wow...  anyway, I hope someone will find this useful.
	- Samuel Audet

	minor simplifications and ugly AU/CDR format stuff by MH

	It's not a very clean code ... Fix this!
*/

#include "mpg123app.h"
#include <errno.h>
#include "debug.h"

#ifdef FLOATOUT
#define WAVE_FORMAT 3
#else
#define WAVE_FORMAT 1
#endif

struct
{
	byte riffheader[4];
	byte WAVElen[4]; /* should this include riffheader or not? */
	struct
	{
		byte WAVEID[4];
		byte fmtheader[4];
		byte fmtlen[4];
		struct
		{
			byte FormatTag[2];
			byte Channels[2];
			byte SamplesPerSec[4];
			byte AvgBytesPerSec[4];
			byte BlockAlign[2];
			byte BitsPerSample[2]; /* format specific for PCM */
			#ifdef FLOATOUT
			byte cbSize[2];
			#endif
		} fmt;
		#ifdef FLOATOUT
		byte factheader[4];
		byte factlen[4];
		struct
		{
			byte samplelen[4];
		} fact;
		#endif
		struct
		{
			byte dataheader[4];
			byte datalen[4];
			/* from here you insert your PCM data */
		} data;
	} WAVE;
} RIFF = 
{
	{ 'R','I','F','F' } ,
	{ sizeof(RIFF.WAVE),0,0,0 } , 
	{
		{ 'W','A','V','E' },
		{ 'f','m','t',' ' },
		{ sizeof(RIFF.WAVE.fmt),0,0,0 } ,
		{
			{WAVE_FORMAT,0} , {0,0},{0,0,0,0},{0,0,0,0},{0,0},{0,0}
			#ifdef FLOATOUT
			,{0,0}
			#endif
		} ,
		#ifdef FLOATOUT
		{ 'f','a','c','t' },
		{ sizeof(RIFF.WAVE.fact),0,0,0 },
		{
			{0,0,0,0} /* to be filled later, like datalen and wavelen */
		},
		#endif
		{ { 'd','a','t','a' }  , {0,0,0,0} }
	}
};

struct auhead {
  byte magic[4];
  byte headlen[4];
  byte datalen[4];
  byte encoding[4];
  byte rate[4];
  byte channels[4];
  byte dummy[8];
} auhead = { 
  { 0x2e,0x73,0x6e,0x64 } , { 0x00,0x00,0x00,0x20 } , 
  { 0xff,0xff,0xff,0xff } , { 0,0,0,0 } , { 0,0,0,0 } , { 0,0,0,0 } , 
  { 0,0,0,0,0,0,0,0 }};


static FILE *wavfp;
static int header_written = 0; /* prevent writing multiple headers to stdout */
static long datalen = 0;
static int flipendian=0;

/* Convertfunctions: */
/* always little endian */

static void long2littleendian(long inval,byte *outval,int b)
{
  int i;
  for(i=0;i<b;i++) {
    outval[i] = (inval>>(i*8)) & 0xff;
  } 
}

/* always big endian */
static void long2bigendian(long inval,byte *outval,int b)
{
  int i;
  for(i=0;i<b;i++) {
    outval[i] = (inval>>((b-i-1)*8)) & 0xff;
  }
}

#ifdef FLOATOUT
static long from_little(byte *inval, int b)
{
	long ret = 0;
	int i;
	for(i=0;i<b;++i) ret += ((long)inval[i])<<(i*8);

	return ret;
}
#endif

static int testEndian(void) 
{
  long i,a=0,b=0,c=0;
  int ret = 0;

  for(i=0;i<sizeof(long);i++) {
    ((byte *)&a)[i] = i;
    b<<=8;
    b |= i;
    c |= i << (i*8);
  }
  if(a == b)
      ret = 1;
  else if(a != c) {
      error3("Strange endianess?? %08lx %08lx %08lx\n",a,b,c);
      ret = -1;
  }
  return ret;
}

static int open_file(char *filename)
{
#if defined(HAVE_SETUID) && defined(HAVE_GETUID)
   setuid(getuid()); /* dunno whether this helps. I'm not a security expert */
#endif
   if(!strcmp("-",filename))  {
      wavfp = stdout;
   }
   else {
     wavfp = fopen(filename,"wb");
     if(!wavfp)
       return -1;
   }
  return 0;
}

static void close_file(void)
{
	if(wavfp != NULL && wavfp != stdout)
	fclose(wavfp);

	wavfp = NULL;
}

/* Wrapper over header writing; ensure that stdout doesn't get multiple headers. */
static size_t write_header(const void*ptr, size_t size)
{
	if(wavfp == stdout)
	{
		if(header_written) return 1;

		header_written = 1;
	}
	return fwrite(ptr, size, 1, wavfp);
}

int au_open(audio_output_t *ao)
{
#ifdef FLOATOUT
	error("AU file support for float values not there yet");
	return -1;
#else
  flipendian = 0;

	if(ao->format < 0) ao->format = MPG123_ENC_SIGNED_16;
	if(ao->rate < 0) ao->rate = 44100;
	if(ao->channels < 0) ao->channels = 2;

  switch(ao->format) {
    case MPG123_ENC_SIGNED_16:
      {
        int endiantest = testEndian();
        if(endiantest == -1) return -1;
        flipendian = !endiantest; /* big end */
        long2bigendian(3,auhead.encoding,sizeof(auhead.encoding));
      }
      break;
    case MPG123_ENC_UNSIGNED_8:
      ao->format = MPG123_ENC_ULAW_8; 
    case MPG123_ENC_ULAW_8:
      long2bigendian(1,auhead.encoding,sizeof(auhead.encoding));
      break;
    default:
      error("AU output is only a hack. This audio mode isn't supported yet.");
      return -1;
  }

  long2bigendian(0xffffffff,auhead.datalen,sizeof(auhead.datalen));
  long2bigendian(ao->rate,auhead.rate,sizeof(auhead.rate));
  long2bigendian(ao->channels,auhead.channels,sizeof(auhead.channels));

  if(open_file(ao->device) < 0)
    return -1;

	write_header(&auhead, sizeof(auhead));
  datalen = 0;

  return 0;
#endif
}

int cdr_open(audio_output_t *ao)
{
#ifdef FLOATOUT
	error("refusing to produce cdr file with float values");
	return -1;
#else
	if(ao->format < 0 && ao->rate < 0 && ao->channels < 0)
	{
  /* param.force_stereo = 0; */
  ao->format = MPG123_ENC_SIGNED_16;
  ao->rate = 44100;
  ao->channels = 2;
	}
  if(ao->format != MPG123_ENC_SIGNED_16 || ao->rate != 44100 || ao->channels != 2) {
    fprintf(stderr,"Oops .. not forced to 16 bit, 44kHz?, stereo\n");
    return -1;
  }

  flipendian = !testEndian(); /* big end */
  

  if(open_file(ao->device) < 0)
    return -1;

  return 0;
#endif
}

int wav_open(audio_output_t *ao)
{
   int bps;

   if(ao->format < 0) ao->format = MPG123_ENC_SIGNED_16;

   flipendian = 0;

   /* standard MS PCM, and its format specific is BitsPerSample */
   long2littleendian(WAVE_FORMAT,RIFF.WAVE.fmt.FormatTag,sizeof(RIFF.WAVE.fmt.FormatTag));
#ifdef FLOATOUT
   long2littleendian(bps=32,RIFF.WAVE.fmt.BitsPerSample,sizeof(RIFF.WAVE.fmt.BitsPerSample));
   flipendian = testEndian();
#else
   if(ao->format == MPG123_ENC_SIGNED_16) {
      long2littleendian(bps=16,RIFF.WAVE.fmt.BitsPerSample,sizeof(RIFF.WAVE.fmt.BitsPerSample));
      flipendian = testEndian();
   }
   else if(ao->format == MPG123_ENC_UNSIGNED_8)
      long2littleendian(bps=8,RIFF.WAVE.fmt.BitsPerSample,sizeof(RIFF.WAVE.fmt.BitsPerSample));
   else
   {
      error("Format not supported.");
      return -1;
   }
#endif

   if(ao->rate < 0) ao->rate = 44100;
   if(ao->channels < 0) ao->channels = 2;

   long2littleendian(ao->channels,RIFF.WAVE.fmt.Channels,sizeof(RIFF.WAVE.fmt.Channels));
    long2littleendian(ao->rate,RIFF.WAVE.fmt.SamplesPerSec,sizeof(RIFF.WAVE.fmt.SamplesPerSec));
   long2littleendian((int)(ao->channels * ao->rate * bps)>>3,
         RIFF.WAVE.fmt.AvgBytesPerSec,sizeof(RIFF.WAVE.fmt.AvgBytesPerSec));
   long2littleendian((int)(ao->channels * bps)>>3,
         RIFF.WAVE.fmt.BlockAlign,sizeof(RIFF.WAVE.fmt.BlockAlign));

   if(open_file(ao->device) < 0)
     return -1;

   long2littleendian(datalen,RIFF.WAVE.data.datalen,sizeof(RIFF.WAVE.data.datalen));
   long2littleendian(datalen+sizeof(RIFF.WAVE),RIFF.WAVElen,sizeof(RIFF.WAVElen));
   if(write_header(&RIFF, sizeof(RIFF)) != 1)
   {
			error1("cannot write header: %s", strerror(errno));
			close_file();
			return -1;
   }

   datalen = 0;
   
   return 0;
}

int wav_write(unsigned char *buf,int len)
{
   int temp;
   int i;

   if(!wavfp) 
     return 0;
  
   if(flipendian) {
#ifdef FLOATOUT
		if(len & 3)
		{
			error("Number of bytes no multiple of 4 (32bit)!");
			return 0;
		}
		for(i=0;i<len;i+=4)
		{
			int j;
			unsigned char tmp[4];
			for(j = 0; j<=3; ++j) tmp[j] = buf[i+j];
			for(j = 0; j<=3; ++j) buf[i+j] = tmp[3-j];
		}
#else
     if(len & 1) {
       error("Odd number of bytes!");
       return 0;
     }
     for(i=0;i<len;i+=2) {
       unsigned char tmp;
       tmp = buf[i+0];
       buf[i+0] = buf[i+1];
       buf[i+1] = tmp;
     }
#endif
   }

   temp = fwrite(buf, 1, len, wavfp);
   if(temp <= 0)
     return 0;
     
   datalen += temp;

   return temp;
}

int wav_close(void)
{
   if(!wavfp) 
      return 0;
   if(fseek(wavfp, 0L, SEEK_SET) >= 0) {
     long2littleendian(datalen,RIFF.WAVE.data.datalen,sizeof(RIFF.WAVE.data.datalen));
     long2littleendian(datalen+sizeof(RIFF.WAVE),RIFF.WAVElen,sizeof(RIFF.WAVElen));
#ifdef FLOATOUT
     long2littleendian(datalen/(from_little(RIFF.WAVE.fmt.Channels,2)*from_little(RIFF.WAVE.fmt.BitsPerSample,2)/8),
                       RIFF.WAVE.fact.samplelen,sizeof(RIFF.WAVE.fact.samplelen));
#endif
     /* Always (over)writing the header here; also for stdout, when fseek worked, this overwrite works. */
     fwrite(&RIFF, sizeof(RIFF),1,wavfp);
   }
   else {
     warning("Cannot rewind WAV file. File-format isn't fully conform now.");
   }
	close_file();
   return 0;
}

int au_close(void)
{
   if(!wavfp)
      return 0;

   if(fseek(wavfp, 0L, SEEK_SET) >= 0) {
     long2bigendian(datalen,auhead.datalen,sizeof(auhead.datalen));
     /* Always (over)writing the header here; also for stdout, when fseek worked, this overwrite works. */
     fwrite(&auhead, sizeof(auhead),1,wavfp); 
   }
   else
   warning("Cannot rewind AU file. File-format isn't fully conform now.");

	close_file();

  return 0;
}

int cdr_close(void)
{
	if(!wavfp) return 0;

	close_file();
	return 0;
}



