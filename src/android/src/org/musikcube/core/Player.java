package org.musikcube.core;

import java.util.ArrayList;

import org.musikcube.core.IQuery.OnQueryResultListener;

import android.content.Intent;

public class Player implements TrackPlayer.OnTrackStatusListener{

	private ArrayList<Integer> nowPlaying	= new ArrayList<Integer>();
	private int position	= 0;
	private Library library;
	
	private java.lang.Object lock	= new java.lang.Object();
	
	private ArrayList<TrackPlayer> playingTracks	= new ArrayList<TrackPlayer>(); 
	private TrackPlayer currentPlayer;
	private TrackPlayer nextPlayer;
	
	public android.app.Service service;
	
	public void run() {
	}
	
	private static org.musikcube.core.Player player = null;
	
	public static final synchronized Player GetInstance(){
		if(Player.player==null){
			Player.player = new org.musikcube.core.Player();
		}
		return Player.player;
	}
	
	public void Play(java.util.ArrayList<Integer> playlist,int position){
		
		synchronized(this.lock){
			this.nowPlaying	= playlist;
			this.position	= position;
		}

		this.Play();
	}
	
	private TrackPlayer PrepareTrack(int position){
		synchronized(this.lock){
			if(this.nowPlaying.size()>position && position>=0){
				int trackId	= this.nowPlaying.get(position);
				String url	= "http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+trackId+"&auth_key="+this.library.authorization;
				TrackPlayer	player	= new TrackPlayer(url,trackId);
				return player;
			}
		}
		return null;
	}
	
	public void Play(){
		this.Startup();
		this.StopAllTracks();

		TrackPlayer newPlayer	= null;
		synchronized(this.lock){
			if(this.nowPlaying.size()>position && position>=0){
				int trackId	= this.nowPlaying.get(position); 
				if(this.nextPlayer!=null){
					if(this.nextPlayer.trackId==trackId){
						newPlayer	= this.nextPlayer;
						this.nextPlayer	= null;
					}else{
						// Something wrong here, not the prepared track
						this.nextPlayer.Stop();
						this.nextPlayer	= null;
					}
				}
				if(newPlayer==null){
					newPlayer	= this.PrepareTrack(this.position);
				}
				this.playingTracks.add(newPlayer);
				this.currentPlayer	= newPlayer;
				newPlayer.listener	= this;
				newPlayer.Play();
				
				if(this.listener!=null){
					this.listener.OnTrackUpdate();
					this.listener.OnTrackBufferUpdate(0);
					this.listener.OnTrackPositionUpdate(0);
				}
			}
		}
		
	}

	///////////////////////////////
	// Interface for updated track
	public interface OnTrackUpdateListener{
		public void OnTrackUpdate();
		public void OnTrackBufferUpdate(int percent);
		public void OnTrackPositionUpdate(int secondsPlayed);
	}
	protected OnTrackUpdateListener listener	= null;
	public void SetUpdateListener(OnTrackUpdateListener listener){
		synchronized(this.lock){
			this.listener	= listener;
			if(this.listener!=null){
				this.listener.OnTrackUpdate();
				this.listener.OnTrackBufferUpdate(0);
				this.listener.OnTrackPositionUpdate(0);
			}
		}
	}
	
	
	public void Next(){
		synchronized(this.lock){
			this.position++;
			if(this.position>=this.nowPlaying.size()){
				this.StopAllTracks();
				this.End();
			}else{
				this.Play();
			}
		}
	}
	
	public void Stop(){
		synchronized(this.lock){
			this.StopAllTracks();
			this.End();
		}
	}
	
	private void StopAllTracks(){
		synchronized(this.lock){
			this.currentPlayer	= null;
			int trackCount	= this.playingTracks.size();
			for(int i=0;i<trackCount;i++){
				this.playingTracks.get(i).listener	= null;
				this.playingTracks.get(i).Stop();
			}
			this.playingTracks.clear();
		}
	}
	
	private void Startup(){
		synchronized(this.lock){
			if(this.library==null){
				this.library	= Library.GetInstance();
				this.library.AddPointer();
			}
		}
	}
	
	private void End(){
		synchronized(this.lock){
			if(this.library!=null){
				this.library.RemovePointer();
				this.library	= null;
/*				
				Intent intent	= new Intent(this.service, org.musikcube.Service.class);
				intent.putExtra("org.musikcube.Service.action", "player ended");
				this.service.startService(intent);
*/
			}
		}
	}

	public void OnTrackStatusUpdate(TrackPlayer trackPlayer,int status) {
		if(status==TrackPlayer.STATUS_EXIT){
			this.Next();
		}
/*		Intent intent	= new Intent(this.service, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.action", "next");
		this.service.startService(intent);*/
	}
	
	public int GetCurrentTrackId(){
		synchronized(this.lock){
			if(this.position>=0 && this.position<this.nowPlaying.size()){
				return this.nowPlaying.get(this.position);
			}
			return 0;
		}
		
	}
	
	public int GetTrackPosition(){
		synchronized(this.lock){
			TrackPlayer player	= this.currentPlayer;
			if(player!=null){
				return player.GetTrackPosition();
			}
		}
		return 0;
	}
	
	public int GetTrackBuffer(){
		synchronized(this.lock){
			TrackPlayer player	= this.currentPlayer;
			if(player!=null){
				return player.GetBuffer();
			}
		}
		return 0;
	}

	public void OnTrackAlmostDone(TrackPlayer trackPlayer) {
		synchronized(this.lock){
			if(this.nextPlayer==null){
				this.nextPlayer	= this.PrepareTrack(this.position+1);
			}
		}
	}

}
