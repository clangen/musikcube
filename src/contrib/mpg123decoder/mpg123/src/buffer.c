/*
	buffer.c: output buffer

	copyright 1997-2009 by the mpg123 project - free software under the terms of the LGPL 2.1
	see COPYING and AUTHORS files in distribution or http://mpg123.org
	initially written by Oliver Fromme

	I (ThOr) am reviewing this file at about the same daytime as Oliver's timestamp here:
	Mon Apr 14 03:53:18 MET DST 1997
	- dammed night coders;-)
*/

#include "mpg123app.h"

#ifndef NOXFERMEM

#include "common.h"
#include <errno.h>
#include "debug.h"

int outburst = 32768;

static int intflag = FALSE;
static int usr1flag = FALSE;

static void catch_interrupt (void)
{
	intflag = TRUE;
}

static void catch_usr1 (void)
{
	usr1flag = TRUE;
}

/* Interfaces to writer process */

extern void buffer_sig(int signal, int block);

void real_buffer_ignore_lowmem(void)
{
	if (!buffermem)
		return;
	if(buffermem->wakeme[XF_READER])
		xfermem_putcmd(buffermem->fd[XF_WRITER], XF_CMD_WAKEUP);
}

void real_buffer_end(int rude)
{
	if (!buffermem)
		return;
	xfermem_putcmd(buffermem->fd[XF_WRITER], rude ? XF_CMD_ABORT : XF_CMD_TERMINATE);
}

void real_buffer_resync(void)
{
	if(buffermem->justwait)
	{
		buffermem->wakeme[XF_WRITER] = TRUE;
		xfermem_putcmd(buffermem->fd[XF_WRITER], XF_CMD_RESYNC);
		xfermem_getcmd(buffermem->fd[XF_WRITER], TRUE);
	}
	else buffer_sig(SIGINT, TRUE);
}

void real_plain_buffer_resync(void)
{
	buffer_sig(SIGINT, FALSE);
}

void real_buffer_reset(void)
{
	buffer_sig(SIGUSR1, TRUE);
}

void real_buffer_start(void)
{
	if(buffermem->justwait)
	{
		debug("ending buffer's waiting");
		buffermem->justwait = FALSE;
		xfermem_putcmd(buffermem->fd[XF_WRITER], XF_CMD_WAKEUP);
	}
}

void real_buffer_stop()
{
	buffermem->justwait = TRUE;
	buffer_sig(SIGINT, TRUE);
}

extern int buffer_pid;

void buffer_sig(int signal, int block)
{
	if (!buffermem) return;

	if (!block)
	{ /* Just signal, do not wait for anything. */
		kill(buffer_pid, signal);
		return;
	}

	/* kill() and the waiting needs to be taken care of properly for parallel execution.
	  Nobody reported issues so far, but I want to be sure. */
	if(xfermem_sigblock(XF_WRITER, buffermem, buffer_pid, signal) != XF_CMD_WAKEUP) 
		perror("Could not resync/reset buffers");
	return;
}

void buffer_loop(audio_output_t *ao, sigset_t *oldsigset)
{
	int bytes, outbytes;
	int my_fd = buffermem->fd[XF_READER];
	txfermem *xf = buffermem;
	int done = FALSE;
	int preload;

	catchsignal (SIGINT, catch_interrupt);
	catchsignal (SIGUSR1, catch_usr1);
	sigprocmask (SIG_SETMASK, oldsigset, NULL);

	xfermem_putcmd(my_fd, XF_CMD_WAKEUP);

	debug("audio output: waiting for cap requests");
	/* wait for audio setup queries */
	while(1)
	{
		int cmd;
		cmd = xfermem_block(XF_READER, xf);
		if(cmd == XF_CMD_AUDIOCAP)
		{
			ao->rate     = xf->rate;
			ao->channels = xf->channels;
			ao->format   = ao->get_formats(ao);
			debug3("formats for %liHz/%ich: 0x%x", ao->rate, ao->channels, ao->format);
			xf->format = ao->format;
			xfermem_putcmd(my_fd, XF_CMD_AUDIOCAP);
		}
		else if(cmd == XF_CMD_WAKEUP)
		{
			debug("got wakeup... leaving config mode");
			xfermem_putcmd(buffermem->fd[XF_READER], XF_CMD_WAKEUP);
			break;
		}
		else
		{
			error1("unexpected command %i", cmd);
			return;
		}
	}

	/* Fill complete buffer on first run before starting to play.
	 * Live mp3 streams constantly approach buffer underrun otherwise. [dk]
	 */
	preload = (int)(param.preload*xf->size);
	if(preload > xf->size) preload = xf->size;
	if(preload < 0) preload = 0;

	for (;;) {
		if (intflag) {
			debug("handle intflag... flushing");
			intflag = FALSE;
			ao->flush(ao);
			/* Either prepare for waiting or empty buffer now. */
			if(!xf->justwait) xf->readindex = xf->freeindex;
			else
			{
				int cmd;
				debug("Prepare for waiting; draining command queue. (There's a lot of wakeup commands pending, usually.)");
				do
				{
					cmd = xfermem_getcmd(my_fd, FALSE);
					/* debug1("drain: %i",  cmd); */
				} while(cmd > 0);
			}
			if(xf->wakeme[XF_WRITER]) xfermem_putcmd(my_fd, XF_CMD_WAKEUP);
		}
		if (usr1flag) {
			debug("handling usr1flag");
			usr1flag = FALSE;
			/*   close and re-open in order to flush
			 *   the device's internal buffer before
			 *   changing the sample rate.   [OF]
			 */
			/* writer must block when sending SIGUSR1
			 * or we will lose all data processed 
			 * in the meantime! [dk]
			 */
			xf->readindex = xf->freeindex;
			/* We've nailed down the new starting location -
			 * writer is now safe to go on. [dk]
			 */
			if (xf->wakeme[XF_WRITER])
				xfermem_putcmd(my_fd, XF_CMD_WAKEUP);
			ao->rate = xf->rate; 
			ao->channels = xf->channels; 
			ao->format = xf->format;
			if (reset_output(ao) < 0) {
				error1("failed to reset audio: %s", strerror(errno));
				exit(1);
			}
		}
		if ( (bytes = xfermem_get_usedspace(xf)) < outburst ) {
			/* if we got a buffer underrun we first
			 * fill 1/8 of the buffer before continue/start
			 * playing */
			if (preload < xf->size>>3)
				preload = xf->size>>3;
			if(preload < outburst)
				preload = outburst;
		}
		debug1("bytes: %i", bytes);
		if(xf->justwait || bytes < preload) {
			int cmd;
			if (done && !bytes) { 
				break;
			}
			
			if(xf->justwait || !done) {

				/* Don't spill into errno check below. */
				errno = 0;
				cmd = xfermem_block(XF_READER, xf);
				debug1("got %i", cmd);
				switch(cmd) {

					/* More input pending. */
					case XF_CMD_WAKEUP_INFO:
						continue;
					/* Yes, we know buffer is low but
					 * know we don't care.
					 */
					case XF_CMD_WAKEUP:
						break;	/* Proceed playing. */
					case XF_CMD_ABORT: /* Immediate end, discard buffer contents. */
						return; /* Cleanup happens outside of buffer_loop()*/
					case XF_CMD_TERMINATE: /* Graceful end, playing stuff in buffer and then return. */
						debug("going to terminate");
						done = TRUE;
						break;
					case XF_CMD_RESYNC:
						debug("ordered resync");
						if (param.outmode == DECODE_AUDIO) ao->flush(ao);

						xf->readindex = xf->freeindex;
						if (xf->wakeme[XF_WRITER]) xfermem_putcmd(my_fd, XF_CMD_WAKEUP);
						continue;
						break;
					case -1:
						if(intflag || usr1flag) /* Got signal, handle it at top of loop... */
						{
							debug("buffer interrupted");
							continue;
						}
						if(errno)
							error1("Yuck! Error in buffer handling... or somewhere unexpected: %s", strerror(errno));
						done = TRUE;
						xf->readindex = xf->freeindex;
						xfermem_putcmd(xf->fd[XF_READER], XF_CMD_TERMINATE);
						break;
					default:
						fprintf(stderr, "\nEh!? Received unknown command 0x%x in buffer process.\n", cmd);
				}
			}
		}
		/* Hack! The writer issues XF_CMD_WAKEUP when first adjust 
		 * audio settings. We do not want to lower the preload mark
		 * just yet!
		 */
		if (xf->justwait || !bytes)
			continue;
		preload = outburst; /* set preload to lower mark */
		if (bytes > xf->size - xf->readindex)
			bytes = xf->size - xf->readindex;
		if (bytes > outburst)
			bytes = outburst;

		debug("write");
		outbytes = flush_output(ao, (unsigned char*) xf->data + xf->readindex, bytes);

		if(outbytes < bytes)
		{
			if(outbytes < 0) outbytes = 0;
			if(!intflag && !usr1flag) {
				error1("Ouch ... error while writing audio data: %s", strerror(errno));
				/*
				 * done==TRUE tells writer process to stop
				 * sending data. There might be some latency
				 * involved when resetting readindex to 
				 * freeindex so we might need more than one
				 * cycle to terminate. (The number of cycles
				 * should be finite unless I managed to mess
				 * up something. ;-) [dk]
				 */
				done = TRUE;	
				xf->readindex = xf->freeindex;
				xfermem_putcmd(xf->fd[XF_READER], XF_CMD_TERMINATE);
			}
			else debug("buffer interrupted");
		}
		bytes = outbytes;

		xf->readindex = (xf->readindex + bytes) % xf->size;
		if (xf->wakeme[XF_WRITER])
			xfermem_putcmd(my_fd, XF_CMD_WAKEUP);
	}
}

#endif

/* EOF */
