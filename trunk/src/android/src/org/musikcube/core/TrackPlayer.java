package org.musikcube.core;

import android.media.MediaPlayer;
import android.util.Log;

public class TrackPlayer implements Runnable, MediaPlayer.OnCompletionListener, MediaPlayer.OnErrorListener,MediaPlayer.OnBufferingUpdateListener {

	public int trackId	= 0;
	private Thread	thread;
	private String url;
	private java.lang.Object lock	= new java.lang.Object();
	private MediaPlayer mediaPlayer;
	private int buffer	= 0;
	private boolean almostDoneSend	= false;
	
	private int status = 1;
	
	public static final int STATUS_PREPARED	= 1;
	public static final int STATUS_PLAYING	= 2;
	public static final int STATUS_PAUSE = 3;
	public static final int STATUS_EXIT	= 10;
	
	public void run() {
		Log.v("mC2::TrackPlayer", "Thread started "+this.url);
		synchronized(this.lock){
			this.mediaPlayer	= new MediaPlayer();
		}
		try {
			
			this.mediaPlayer.setOnCompletionListener(this);
			this.mediaPlayer.setOnErrorListener(this);
			this.mediaPlayer.setOnBufferingUpdateListener(this);
			
			this.mediaPlayer.setDataSource(this.url);
			Log.v("mC2::TrackPlayer", "Preparing "+this.url);
			this.mediaPlayer.prepare();
			Log.v("mC2::TrackPlayer", "Prepared "+this.url);
			
			synchronized(this.lock){
				if(this.listener!=null){
					this.listener.OnTrackPrepared(this);
				}
				
				while(this.status==STATUS_PREPARED)
					this.lock.wait();
			}
			
			int currentStatus	= 0;
			synchronized(this.lock){
				currentStatus	= this.status;
			}

			Log.v("mC2::TrackPlayer", "Start? "+this.url);
			
			if(currentStatus==STATUS_PLAYING)
				this.mediaPlayer.start();
		
			synchronized(this.lock){
				while(this.status==STATUS_PLAYING){
					if(!this.almostDoneSend){
						int duration	= this.mediaPlayer.getDuration();
						int position	= this.mediaPlayer.getCurrentPosition();
						if(duration>0 && position+10000>duration){
							// The track is almost done
							this.almostDoneSend	= true;
							if(this.listener!=null){
								Log.v("mC2::TrackPlayer", "OnTrackAlmostDone "+this.url);
								this.listener.OnTrackAlmostDone(this);
							}
						}
					}
					this.lock.wait(3000);
					
				}
			}
			Log.v("mC2::TrackPlayer", "Stopping "+this.url);
			
			this.mediaPlayer.stop();
			this.mediaPlayer.release();
			synchronized(this.lock){
				this.mediaPlayer	= null;
			}
		
		} catch (Exception e) {
		}
		synchronized(this.lock){
			this.status	= STATUS_EXIT;
		}
		Log.v("mC2::TrackPlayer", "ExitCallbacks "+this.url);
		
		this.CallListener();
		Log.v("mC2::TrackPlayer", "END "+this.url);
		
	}

	public TrackPlayer(String url,int trackId){
		this.trackId	= trackId;
		this.url	= url;
		this.thread	= new Thread(this);
		this.thread.start();
	}
	
/*	public TrackPlayer(String url,boolean start){
		this.url	= url;
		if(start==true){
			this.status	= STATUS_PLAYING;
		}
		this.thread	= new Thread(this);
		this.thread.start();
	}*/
	
	private void Exit(){
		synchronized(this.lock){
			this.status	= STATUS_EXIT;
			this.lock.notifyAll();
		}
		try {
			this.thread.join();
		} catch (InterruptedException e) {
		}
	}
	
	public void Stop(){
		synchronized(this.lock){
			this.status	= STATUS_EXIT;
			this.lock.notifyAll();
		}
	}
	
	public void Pause(){
		synchronized(this.lock){
			this.status	= STATUS_PAUSE;
			this.lock.notifyAll();
		}
	}
	
	public void Play(){
		synchronized(this.lock){
			if(this.status==STATUS_PLAYING || this.status==STATUS_PREPARED){
				this.status = STATUS_PLAYING;
			}
			this.lock.notifyAll();
		}
	}
	
	private void CallListener(){
		synchronized(this.lock){
			if(this.listener!=null){
				this.listener.OnTrackStatusUpdate(this,this.status);
			}
		}
	}

	@Override
	protected void finalize() throws Throwable {
		this.Exit();
		super.finalize();
	}
	
	public interface OnTrackStatusListener{
		public void OnTrackPrepared(TrackPlayer trackPlayer);
		public void OnTrackStatusUpdate(TrackPlayer trackPlayer,int status);
		public void OnTrackAlmostDone(TrackPlayer trackPlayer);
	}
	
	private OnTrackStatusListener listener	= null;
	
	public void SetListener(OnTrackStatusListener listener){
		synchronized(this.lock){
			this.listener	= listener;
		}
	}

	public void onCompletion(MediaPlayer mp) {
		synchronized(this.lock){
			this.status	= STATUS_EXIT;
			this.lock.notifyAll();
		}
	}

	public boolean onError(MediaPlayer mp, int what, int extra) {
		synchronized(this.lock){
			this.status	= STATUS_EXIT;
			this.lock.notifyAll();
		}
		return false;
	}
	
	public int GetTrackPosition(){
		synchronized(this.lock){
			if(this.mediaPlayer!=null){
				return this.mediaPlayer.getCurrentPosition();
			}
		}
		return 0;
	}

	public void onBufferingUpdate(MediaPlayer mp, int percent) {
		synchronized(this.lock){
			this.buffer	= percent;
		}
	}
	
	public int GetBuffer(){
		synchronized(this.lock){
			return this.buffer;
		}
	}
	
}
