package org.musikcube;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import org.musikcube.core.IQuery;
import org.musikcube.core.Library;
import org.musikcube.core.Player;
import org.musikcube.core.Track;
import org.musikcube.core.IQuery.OnQueryResultListener;
import org.musikcube.core.Player.OnTrackUpdateListener;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.SeekBar;
import android.widget.TextView;

public class PlayerControl extends Activity implements OnTrackUpdateListener, OnQueryResultListener {

	private Track track		= new Track();
	private int duration = 0;
	private Object lock	= new Object();
	
    @Override
    public void onCreate(Bundle savedInstanceState) {
		Log.v("MC2::PC","OnCreate");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.play_control);
        
        ImageButton nextButton	= (ImageButton)findViewById(R.id.MediaNext);
        nextButton.setOnClickListener(this.onNextClick);
        ImageButton pauseButton	= (ImageButton)findViewById(R.id.MediaPause);
        pauseButton.setOnClickListener(this.onPauseClick);
        
		this.callbackTrackPositionsUpdateHandler.postDelayed(callbackTrackPositionsUpdateRunnable,500);
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
		}
		this.callbackTrackPositionsUpdateHandler.post(this.callbackTrackPositionsUpdateRunnable);
	}
	public void OnTrackPositionUpdate(int secondsPlayed) {
		synchronized(lock){
		}
		this.callbackTrackPositionsUpdateHandler.post(this.callbackTrackPositionsUpdateRunnable);
	}
	public void OnTrackUpdate() {
/*		
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
		}*/
		this.callbackTrackUpdateHandler.post(this.callbackTrackUpdateRunnable);
	}
	
	
	public void OnQueryResults(IQuery query) {
/*		MetadataQuery mdQuery	= (MetadataQuery)query;
		if(!mdQuery.resultTracks.isEmpty()){
			synchronized(lock){
				Track newTrack	= mdQuery.resultTracks.get(0);
				if(this.trackId==newTrack.id){
					this.track	= newTrack;
					this.callbackTrackUpdateHandler.post(this.callbackTrackUpdateRunnable);
				}
			}
		}*/
	}
	
	@Override
	protected void onPause() {
		Log.v("MC2::PC","OnPause");
		Player.GetInstance().SetUpdateListener(null);
		super.onPause();
	}
	@Override
	protected void onResume() {
		Log.v("MC2::PC","OnResume");
		Player.GetInstance().SetUpdateListener(this);
		super.onResume();
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
		ImageView cover	= (ImageView)findViewById(R.id.AlbumCover);
		cover.setImageResource(R.drawable.album);

		if(thumbnailId!=0){
			// Load image
			Library library	= Library.GetInstance();
			new DownloadAlbumCoverTask().execute("http://"+library.host+":"+library.httpPort+"/cover/?cover_id="+thumbnailId);
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
				Log.v("mC2:PLAYER","Error "+e.getMessage());
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
		
		// Next callback in 0.5 seconds
		this.callbackTrackPositionsUpdateHandler.postDelayed(callbackTrackPositionsUpdateRunnable,500);

	}
    
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.default_menu, menu);
        return true;
    }    
    
    public boolean onOptionsItemSelected(MenuItem item) {
    	//Log.i("MC2.onContextItemSelected","item "+item.getItemId()+" "+R.id.context_settings);
   	  switch (item.getItemId()) {
		  case R.id.context_settings:
	    		startActivity(new Intent(this, org.musikcube.Preferences.class));
			  return true;
		  case R.id.context_browse:
	    		startActivity(new Intent(this, org.musikcube.main.class));
			  return true;
		  case R.id.context_controls:
	    		startActivity(new Intent(this, org.musikcube.PlayerControl.class));
			  return true;
    	  default:
    		  return super.onContextItemSelected(item);
    	  }
   	}

}
