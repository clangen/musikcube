package org.musikcube;

import java.util.Timer;
import java.util.TimerTask;

import org.musikcube.core.IQuery;
import org.musikcube.core.Library;
import org.musikcube.core.MetadataQuery;
import org.musikcube.core.Player;
import org.musikcube.core.Track;
import org.musikcube.core.IQuery.OnQueryResultListener;
import org.musikcube.core.Player.OnTrackUpdateListener;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.SeekBar;
import android.widget.TextView;

public class PlayerControl extends Activity implements OnTrackUpdateListener, OnQueryResultListener {

	private int trackId		= 0;
	private Track track		= new Track();
	private int buffer	= 0;
	private int duration = 0;
	private int secondsPlayed	= 0;
	private Object lock	= new Object();
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.play_control);
        
        ImageButton nextButton	= (ImageButton)findViewById(R.id.MediaNext);
        nextButton.setOnClickListener(this.onNextClick);
        ImageButton pauseButton	= (ImageButton)findViewById(R.id.MediaPause);
        pauseButton.setOnClickListener(this.onPauseClick);
        
    }

    private OnClickListener onNextClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(PlayerControl.this, org.musikcube.Service.class);
    		intent.putExtra("org.musikcube.Service.action", "next");
    		startService(intent);
    	}
    };
    private OnClickListener onPauseClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(PlayerControl.this, org.musikcube.Service.class);
    		intent.putExtra("org.musikcube.Service.action", "stop");
    		startService(intent);
    	}
    };

	public void OnTrackBufferUpdate(int percent) {
		synchronized(lock){
			this.buffer	= percent;
		}
		this.callbackTrackPositionsUpdateHandler.post(this.callbackTrackPositionsUpdateRunnable);
	}
	public void OnTrackPositionUpdate(int secondsPlayed) {
		synchronized(lock){
			this.secondsPlayed	= secondsPlayed;
		}
		this.callbackTrackPositionsUpdateHandler.post(this.callbackTrackPositionsUpdateRunnable);
	}
	public void OnTrackUpdate() {
		
		synchronized(lock){
			int newTrackId	= Player.GetInstance().GetCurrentTrackId();
			if(newTrackId!=this.trackId){
				this.trackId	= newTrackId;
				this.track	= new Track();
				
				if(this.trackId!=0){
					MetadataQuery query	= new MetadataQuery();
					query.requestedMetakeys.add("title");
					query.requestedMetakeys.add("track");
					query.requestedMetakeys.add("visual_artist");
					query.requestedMetakeys.add("album");
					query.requestedMetakeys.add("year");
					query.requestedMetakeys.add("thumbnail_id");
					query.requestedMetakeys.add("duration");
					query.requestedTracks.add(this.trackId);
					query.SetResultListener(this);
					Library.GetInstance().AddQuery(query);
				}
			}
		}
		this.callbackTrackUpdateHandler.post(this.callbackTrackUpdateRunnable);
	}
	
	
	public void OnQueryResults(IQuery query) {
		MetadataQuery mdQuery	= (MetadataQuery)query;
		if(!mdQuery.resultTracks.isEmpty()){
			synchronized(lock){
				Track newTrack	= mdQuery.resultTracks.get(0);
				if(this.trackId==newTrack.id){
					this.track	= newTrack;
					this.callbackTrackUpdateHandler.post(this.callbackTrackUpdateRunnable);
				}
			}
		}
	}
	
	@Override
	protected void onPause() {
		Player.GetInstance().SetUpdateListener(null);
		this.timer.cancel();
		super.onPause();
	}
	@Override
	protected void onResume() {
		Player.GetInstance().SetUpdateListener(this);
		super.onResume();
		
		this.timer	= new Timer();
		this.timer.schedule(new TimerTask() { public void run() {
			callbackTrackPositionsUpdateHandler.post(callbackTrackPositionsUpdateRunnable);
		} }, 100);
	}
    
	// Need handler for callbacks to the UI thread
    final Handler callbackTrackUpdateHandler = new Handler();
    // Create runnable for posting
    final Runnable callbackTrackUpdateRunnable = new Runnable() {
        public void run() {
            OnUpdateTrackUI();
        }
    };
    
	public void OnUpdateTrackUI() {
		TextView titleView	= (TextView)findViewById(R.id.TrackTitle);
		TextView albumView	= (TextView)findViewById(R.id.TrackAlbum);
		TextView artistView	= (TextView)findViewById(R.id.TrackArtist);
		TextView durationView	= (TextView)findViewById(R.id.TrackDuration);
			
		synchronized(lock){
			String title	= this.track.metadata.get("title");
			if(title==null){
				titleView.setText("Title:");
			}else{
				titleView.setText("Title: "+title);
			}
			String album	= this.track.metadata.get("album");
			if(album==null){
				albumView.setText("Album:");
			}else{
				albumView.setText("Album: "+album);
			}
			String artist	= this.track.metadata.get("visual_artist");
			if(artist==null){
				artistView.setText("Artist:");
			}else{
				artistView.setText("Artist: "+artist);
			}
			
			String duration	= this.track.metadata.get("duration");
			if(duration==null){
				this.duration	= 0;
			}else{
				this.duration	= Integer.parseInt(duration);
			}
			int minutes	= (int)Math.floor(this.duration/60);
			int seconds	= this.duration-minutes*60;
			String durationText	= Integer.toString(minutes)+":";
			if(seconds<10){ durationText	+= "0"; }
			durationText	+= Integer.toString(seconds);
			durationView.setText(durationText);
		}
	}
	
	// Need handler for callbacks to the UI thread
    final Handler callbackTrackPositionsUpdateHandler = new Handler();
    // Create runnable for posting
    final Runnable callbackTrackPositionsUpdateRunnable = new Runnable() {
        public void run() {
            OnUpdateTrackPositionsUI();
        }
    };
    
	public void OnUpdateTrackPositionsUI() {
		int msPosition	= Player.GetInstance().GetTrackPosition();
		int position	= msPosition/1000;
		int minutes	= (int)Math.floor(position/60);
		int seconds	= position-minutes*60;
		String positionText	= Integer.toString(minutes)+":";
		if(seconds<10){ positionText	+= "0"; }
		positionText	+= Integer.toString(seconds);
		TextView positionView	= (TextView)findViewById(R.id.TrackPosition);
		positionView.setText(positionText);

		SeekBar seekBar	= (SeekBar)findViewById(R.id.TrackProgress);
		if(this.duration==0){
			seekBar.setProgress(0);
		}else{
			seekBar.setProgress(msPosition/this.duration);
		}
		
		// Next callback in 0.5 seconds
		this.timer.schedule(new TimerTask() { public void run() {
			callbackTrackPositionsUpdateHandler.post(callbackTrackPositionsUpdateRunnable);
		} }, 500);
	}
	
	private java.util.Timer timer	= new java.util.Timer(); 

	/*
	 gametimer.schedule(new TimerTask() { public void run() {
	     seconds+=0.1; updatecount();
	} }, 100, 100);
	*/	
}
