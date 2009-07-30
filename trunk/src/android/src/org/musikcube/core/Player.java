package org.musikcube.core;

import java.util.ArrayList;

import android.content.Intent;

public class Player implements TrackPlayer.OnTrackStatusListener{

	private ArrayList<Integer> nowPlaying	= new ArrayList<Integer>();
	private int position	= 0;
	private Library library;
	
	private java.lang.Object lock	= new java.lang.Object();
	
	private ArrayList<TrackPlayer> playingTracks	= new ArrayList<TrackPlayer>(); 
	
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
/*
	private synchronized void StartThread(){
		synchronized(lock){
			if(this.running==false){
				this.thread	= new Thread(this);
				this.running	= true;
				this.thread.start();
			}
		}
	}
	*/
	
	public void Play(java.util.ArrayList<Integer> playlist,int position){
		
		synchronized(this.lock){
			this.nowPlaying	= playlist;
			this.position	= position;
		}

		this.Play();
	}
	
	public void Play(){
		this.Startup();
		this.StopAllTracks();
		
		synchronized(this.lock){
			String url	= "http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+this.nowPlaying.get(this.position)+"&auth_key="+this.library.authorization;
			TrackPlayer	player	= new TrackPlayer(url,true);
			player.listener	= this;
			this.playingTracks.add(player);
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
				
				Intent intent	= new Intent(this.service, org.musikcube.Service.class);
				intent.putExtra("org.musikcube.Service.action", "player ended");
				this.service.startService(intent);

			}
		}
	}

	public void OnTrackStatusUpdate(TrackPlayer trackPlayer,int status) {
//		this.Next();
		Intent intent	= new Intent(this.service, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.action", "next");
		this.service.startService(intent);
	}
	

}
