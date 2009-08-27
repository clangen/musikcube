package org.musikcube.core;

import java.util.ArrayList;

import org.musikcube.core.IQuery.OnQueryResultListener;

import android.content.Intent;

public class Player implements TrackPlayer.OnTrackStatusListener, OnQueryResultListener{

	private ArrayList<Integer> nowPlaying	= new ArrayList<Integer>();
	private int position	= 0;
	
	private boolean repeat	= false;
	private boolean shuffle = false;
	
	private Library library;
	
	private java.lang.Object lock	= new java.lang.Object();
	
	private ArrayList<TrackPlayer> playingTracks	= new ArrayList<TrackPlayer>(); 
	private TrackPlayer currentPlayer;
	private TrackPlayer nextPlayer;
	private Track currentTrack	= new Track();
	
	public boolean playWhenPrepared	= false;
	
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

	public void Append(java.util.ArrayList<Integer> playlist){
		
		synchronized(this.lock){
			this.nowPlaying.addAll(playlist);
			if(this.listListener!=null){
				this.listListener.OnTrackListUpdate();
			}
		}
	}
	
	public void PlayWhenPrepared(java.util.ArrayList<Integer> playlist,int position){
		synchronized(this.lock){
			this.nowPlaying	= playlist;
			this.position	= position;
			if(this.nextPlayer!=null){
				this.nextPlayer.SetListener(null);
				this.nextPlayer.Stop();
			}
			this.playWhenPrepared	= true;
			this.nextPlayer	= this.PrepareTrack(this.position);
			this.nextPlayer.SetListener(this);
			
			if(this.listListener!=null){
				this.listListener.OnTrackListUpdate();
			}
			
		}
	}
	
	private TrackPlayer PrepareTrack(int position){
		synchronized(this.lock){
			if(this.nowPlaying.size()>position && position>=0){
				int trackId	= this.nowPlaying.get(position);
				//String url	= "http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+trackId+"&auth_key="+this.library.authorization;
				TrackPlayer	player	= new TrackPlayer(trackId);
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
						this.nextPlayer.SetListener(null);
						this.nextPlayer.Stop();
						this.nextPlayer	= null;
					}
				}
				if(newPlayer==null){
					newPlayer	= this.PrepareTrack(this.position);
				}
				this.playingTracks.add(newPlayer);
				this.currentPlayer	= newPlayer;
				newPlayer.SetListener(this);
				newPlayer.Play();
				
				if(this.listener!=null){
					this.listener.OnTrackUpdate();
					this.listener.OnTrackBufferUpdate(0);
					this.listener.OnTrackPositionUpdate(0);
				}

				// query for metadata
				MetadataQuery query	= new MetadataQuery();
				query.requestedMetakeys.add("title");
				query.requestedMetakeys.add("track");
				query.requestedMetakeys.add("visual_artist");
				query.requestedMetakeys.add("album");
				query.requestedMetakeys.add("year");
				query.requestedMetakeys.add("thumbnail_id");
				query.requestedMetakeys.add("duration");
				query.requestedTracks.add(trackId);
				query.SetResultListener(this);
				Library.GetInstance().AddQuery(query);
				
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
	
	public interface OnTrackListUpdateListener{
		public void OnTrackListPositionUpdate();
		public void OnTrackListUpdate();
	}
	
	
	protected OnTrackUpdateListener listener			= null;
	protected OnTrackListUpdateListener listListener	= null;
	
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
	public void SetListUpdateListener(OnTrackListUpdateListener listener){
		synchronized(this.lock){
			this.listListener	= listener;
			if(this.listListener!=null){
				this.listListener.OnTrackListUpdate();
			}
		}
	}
	
	
	public void Next(){
		synchronized(this.lock){
			this.currentTrack	= new Track();
			this.position++;

			if(this.position>=this.nowPlaying.size()){
				if(this.repeat){
					this.position=0;
					this.Play();
				}else{
					this.StopAllTracks();
					this.End();
				}
			}else{
				this.Play();
			}
			
			if(this.listListener!=null){
				this.listListener.OnTrackListPositionUpdate();
			}
			
		}
	}
	
	public void Prev(){
		synchronized(this.lock){
			this.currentTrack	= new Track();
			this.position--;
			if(this.position<0){
				this.position	= 0;
			}
			if(this.listListener!=null){
				this.listListener.OnTrackListPositionUpdate();
			}
			
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
			if(this.nextPlayer!=null){
				this.nextPlayer.SetListener(null);
				this.nextPlayer.Stop();
				this.nextPlayer	= null;
			}
			this.End();
		}
	}
	
	private void StopAllTracks(){
		synchronized(this.lock){
			this.currentPlayer	= null;
			int trackCount	= this.playingTracks.size();
			for(int i=0;i<trackCount;i++){
				this.playingTracks.get(i).SetListener(null);
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
				
				Intent intent	= new Intent(this.service, org.musikcube.Service.class);
				intent.putExtra("org.musikcube.Service.action", "player_start");
				this.service.startService(intent);
			}
		}
	}
	
	private void End(){
		synchronized(this.lock){
			this.currentTrack	= new Track();
			if(this.listener!=null){
				this.listener.OnTrackUpdate();
				this.listener.OnTrackBufferUpdate(0);
				this.listener.OnTrackPositionUpdate(0);
			}
			if(this.library!=null){
				this.library.RemovePointer();
				this.library	= null;
				
				Intent intent	= new Intent(this.service, org.musikcube.Service.class);
				intent.putExtra("org.musikcube.Service.action", "player_end");
				this.service.startService(intent);

			}
		}
	}

	public void OnTrackStatusUpdate(TrackPlayer trackPlayer,int status) {
/*		if(status==TrackPlayer.STATUS_EXIT){
			this.Next();
		}*/
		Intent intent	= new Intent(this.service, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.action", "next");
		this.service.startService(intent);
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

	public void OnQueryResults(IQuery query) {
		MetadataQuery mdQuery = (MetadataQuery)query;
		synchronized(this.lock){
			this.currentTrack	= mdQuery.resultTracks.get(0);
			if(this.listener!=null){
				this.listener.OnTrackUpdate();
			}
		}
		
		Intent intent	= new Intent(this.service, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.action", "player_start");
		this.service.startService(intent);
	}
	
	public Track GetCurrentTrack(){
		synchronized(this.lock){
			return this.currentTrack;
		}
	}

	public void OnTrackPrepared(TrackPlayer trackPlayer) {
		synchronized(this.lock){
			if(this.playWhenPrepared){
				this.playWhenPrepared	= false;
				this.Play();
			}
		}
	}
	
	public ArrayList<Integer> GetTracklist(){
		synchronized(this.lock){
			ArrayList<Integer> newPlaylist	=(ArrayList<Integer>) this.nowPlaying.clone();
			return newPlaylist;
		}
	}
	
	public int GetPosition(){
		synchronized(this.lock){
			return this.position;
		}
	}

	public void SetRepeat(boolean repeat){
		synchronized(this.lock){
			this.repeat	= repeat;
		}
	}
	public boolean GetRepeat(){
		synchronized(this.lock){
			return this.repeat;
		}
	}
	public void SetShuffle(boolean shuffle){
		synchronized(this.lock){
			this.shuffle	= shuffle;
		}
	}
	public boolean GetShuffle(){
		synchronized(this.lock){
			return this.shuffle;
		}
	}
	
}
