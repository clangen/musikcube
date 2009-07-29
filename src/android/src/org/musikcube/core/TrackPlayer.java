package org.musikcube.core;

import android.media.MediaPlayer;

public class TrackPlayer implements Runnable, MediaPlayer.OnCompletionListener, MediaPlayer.OnErrorListener {

	private Thread	thread;
	private String url;
	private java.lang.Object lock	= new java.lang.Object();
	private MediaPlayer mediaPlayer;
	
	private int status = 1;
	
	private static final int STATUS_PREPARED	= 1;
	private static final int STATUS_PLAYING	= 2;
	private static final int STATUS_PAUSE = 3;
	private static final int STATUS_EXIT	= 10;
	
	public void run() {
		this.mediaPlayer	= new MediaPlayer();
		try {
			
			this.mediaPlayer.setOnCompletionListener(this);
			this.mediaPlayer.setOnErrorListener(this);
			
			this.mediaPlayer.setDataSource(this.url);
			this.mediaPlayer.prepare();
			
			synchronized(this.lock){
				while(this.status==STATUS_PREPARED)
					this.lock.wait();
			}
			
			int currentStatus	= 0;
			synchronized(this.lock){
				currentStatus	= this.status;
			}

			if(currentStatus==STATUS_PLAYING)
				this.mediaPlayer.start();
		
			synchronized(this.lock){
				while(this.status==STATUS_PLAYING)
					this.lock.wait();
			}
			
			this.mediaPlayer.stop();
		
		} catch (Exception e) {
			// TODO Auto-generated catch block
			synchronized(this.lock){
				this.status	= STATUS_EXIT;
			}
		}
		
		this.CallListener();
		
	}

	public TrackPlayer(String url){
		this.url	= url;
		this.thread	= new Thread(this);
		this.thread.start();
	}
	
	public TrackPlayer(String url,boolean start){
		this.url	= url;
		if(start==true){
			this.status	= STATUS_PLAYING;
		}
		this.thread	= new Thread(this);
		this.thread.start();
	}
	
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
	
	public boolean Play(){
		synchronized(this.lock){
			if(this.status==STATUS_PLAYING || this.status==STATUS_PREPARED){
				this.status = STATUS_PLAYING;
				this.lock.notifyAll();
				return true;
			}
		}
		return false;
	}
	
	private void CallListener(){
		if(this.listener!=null){
			this.listener.OnTrackStatusUpdate(this,this.status);
		}
	}

	@Override
	protected void finalize() throws Throwable {
		this.Exit();
		super.finalize();
	}
	
	public interface OnTrackStatusListener{
		public void OnTrackStatusUpdate(TrackPlayer trackPlayer,int status);
	}
	
	public OnTrackStatusListener listener	= null;

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
	
	
}
