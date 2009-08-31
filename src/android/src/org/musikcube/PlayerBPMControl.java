package org.musikcube;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import org.musikcube.core.Library;
import org.musikcube.core.Player;
import org.musikcube.core.Track;
import org.musikcube.core.Workout;
import org.musikcube.core.Player.OnTrackUpdateListener;
import org.musikcube.core.Workout.OnWorkoutListener;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.CompoundButton;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.ToggleButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.SeekBar.OnSeekBarChangeListener;

public class PlayerBPMControl extends Activity implements OnTrackUpdateListener, OnWorkoutListener {

	private Track track		= new Track();
	private int duration = 0;
	private Object lock	= new Object();
	private boolean enable	= false;
	private int currentAlbumCoverId	= 0;
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.play_bpm_control);
        
        ImageButton nextButton	= (ImageButton)findViewById(R.id.MediaNext);
        nextButton.setOnClickListener(this.onNextClick);
        ImageButton pauseButton	= (ImageButton)findViewById(R.id.MediaPause);
        pauseButton.setOnClickListener(this.onPauseClick);
		ToggleButton acc	= (ToggleButton)findViewById(R.id.ToggleAccelerator);
		acc.setOnCheckedChangeListener(this.onAcceleratorToggle);
        SeekBar bpmBar		= (SeekBar)findViewById(R.id.BPM);
        bpmBar.setOnSeekBarChangeListener(this.onBPMChanged);
        
		this.callbackTrackPositionsUpdateHandler.postDelayed(callbackTrackPositionsUpdateRunnable,500);
   }

    private OnClickListener onNextClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(PlayerBPMControl.this, org.musikcube.Service.class);
    		intent.putExtra("org.musikcube.Service.action", "next");
    		startService(intent);
    	}
    };
    private OnClickListener onPauseClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(PlayerBPMControl.this, org.musikcube.Service.class);
    		if(Workout.GetInstance().Active()){
    			intent.putExtra("org.musikcube.Service.action", "workoutstop");
    		}else{
	    		intent.putExtra("org.musikcube.Service.action", "workoutstart");
    		}
    		startService(intent);
//    		PlayerBPMControl.this.OnUpdateTrackUI();
    	}
    };
	private OnCheckedChangeListener onAcceleratorToggle = new OnCheckedChangeListener(){
		public void onCheckedChanged(CompoundButton buttonView,boolean isChecked) {
			Workout.GetInstance().UseAccelerometer(isChecked);
		}
	};
	private OnSeekBarChangeListener onBPMChanged = new OnSeekBarChangeListener(){

		public void onProgressChanged(SeekBar seekBar, int progress,
				boolean fromUser) {
			if(fromUser){
				if(!Workout.GetInstance().Accelerometer()){
					// Seek
					float bpm	= (float) ((float)progress*150.0/1000.0+50.0);
					Workout.GetInstance().SetBPM(bpm);
					
					TextView bpmTitle	= (TextView)PlayerBPMControl.this.findViewById(R.id.BPMTitle);
					bpmTitle.setText("BPM: "+bpm);
				}
			}
		}

		public void onStartTrackingTouch(SeekBar seekBar) {
		}

		public void onStopTrackingTouch(SeekBar seekBar) {
		}
		
	};

    
	public void OnTrackBufferUpdate(int percent) {
		this.callbackTrackPositionsUpdateHandler.post(this.callbackTrackPositionsUpdateRunnable);
	}
	public void OnTrackPositionUpdate(int secondsPlayed) {
		this.callbackTrackPositionsUpdateHandler.post(this.callbackTrackPositionsUpdateRunnable);
	}
	public void OnTrackUpdate() {
		this.callbackTrackUpdateHandler.post(this.callbackTrackUpdateRunnable);
	}
	
	@Override
	protected void onPause() {
		this.enable	= false;
		org.musikcube.core.Library.GetInstance().RemovePointer();
		Player.GetInstance().SetUpdateListener(null);
		Workout.GetInstance().SetListener(null);
		super.onPause();
	}
	@Override
	protected void onResume() {
		this.enable	= true;
		org.musikcube.core.Library.GetInstance().AddPointer();
		Player.GetInstance().SetUpdateListener(this);
		super.onResume();
		this.OnUpdateTrackPositionsUI();
		this.OnUpdateTrackUI();
		
		Workout.GetInstance().SetListener(this);
		
		ToggleButton acc	= (ToggleButton)findViewById(R.id.ToggleAccelerator);
		acc.setChecked(Workout.GetInstance().Accelerometer());
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
		
		int thumbnailId	= 0;
			
		synchronized(lock){
			
			this.track	= Player.GetInstance().GetCurrentTrack();
			if(this.track==null){
				this.track	= new Track();
			}
			
			String thumbnailString		= this.track.metadata.get("thumbnail_id");
			if(thumbnailString!=null){
				thumbnailId	= Integer.parseInt(thumbnailString);
			}
			
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

		// clear image
		if(this.currentAlbumCoverId!=thumbnailId){
			this.currentAlbumCoverId=thumbnailId;
			ImageView cover	= (ImageView)findViewById(R.id.AlbumCover);
			cover.setImageResource(R.drawable.album);
	
			if(thumbnailId!=0){
				// Load image
				Library library	= Library.GetInstance();
				new DownloadAlbumCoverTask().execute("http://"+library.host+":"+library.httpPort+"/cover/?cover_id="+thumbnailId);
			}
		}
		
		// Update play button
		ImageButton playButton	= (ImageButton)findViewById(R.id.MediaPause);
		if(Workout.GetInstance().Active()){
			playButton.setImageResource(R.drawable.ic_media_pause);
		}else{
			playButton.setImageResource(R.drawable.ic_media_play);
		}
		
		
	}
	
	private class DownloadAlbumCoverTask extends AsyncTask<String,Integer,Bitmap>{

		protected Bitmap doInBackground(String... params) {
			try {
				URL url	= new URL(params[0]);
		        HttpURLConnection conn= (HttpURLConnection)url.openConnection();
	            conn.setDoInput(true);
	            conn.connect();
	            //int length = conn.getContentLength();
	            InputStream is	= conn.getInputStream();
	            Bitmap bm	= BitmapFactory.decodeStream(is);
	            return bm;
			} catch (Exception e) {
//				e.printStackTrace();
				return null;
			}
		}
		
		protected void onPostExecute(Bitmap result){
			if(result==null){
			}else{
				ImageView cover	= (ImageView)findViewById(R.id.AlbumCover);
				cover.setImageBitmap(result);
			}
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
		if(this.enable){
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
			synchronized (this.lock) {
				if(this.duration==0){
					seekBar.setProgress(0);
				}else{
					seekBar.setProgress(msPosition/this.duration);
				}
				seekBar.setSecondaryProgress(10*Player.GetInstance().GetTrackBuffer());
			}
		}
		
		// Update BPM
		SeekBar bpmBar	= (SeekBar)findViewById(R.id.BPM);
		final int startBPM	= 50;
		final int endBPM		= 200;
		final float currentBPM	= Workout.GetInstance().GetBPM();
		bpmBar.setProgress( (int) (((float)currentBPM-startBPM)*1000/(endBPM-startBPM)) );
		
		TextView bpmTitle	= (TextView)findViewById(R.id.BPMTitle);
		bpmTitle.setText("BPM: "+currentBPM);
		
		// Next callback in 0.5 seconds
		this.callbackTrackPositionsUpdateHandler.postDelayed(callbackTrackPositionsUpdateRunnable,500);

	}
    
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.default_menu, menu);
        return true;
    }    
    
    public boolean onOptionsItemSelected(MenuItem item) {
    	if(Helper.DefaultOptionsItemSelected(item,this)){
    		return true;
    	}else{
    		return super.onContextItemSelected(item);
    	}
   	}
	public void OnBPMUpdate() {
		this.callbackTrackUpdateHandler.post(this.callbackTrackUpdateRunnable);
	}

}
