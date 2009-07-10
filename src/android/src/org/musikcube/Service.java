/**
 * 
 */
package org.musikcube;

import java.io.IOException;
import java.util.ArrayList;

import org.musikcube.core.Library;

import android.content.Intent;
import android.media.MediaPlayer;
import android.os.IBinder;
import android.util.Log;

/**
 * @author doy
 *
 */
public class Service extends android.app.Service {

	Library library;
	MediaPlayer	player;
	
	ArrayList<Integer> nowPlaying	= new ArrayList<Integer>();
	int nowPlayingPosition			= 0;
	
	/**
	 * 
	 */
	public Service() {
		// TODO Auto-generated constructor stub
	}

	/* (non-Javadoc)
	 * @see android.app.Service#onBind(android.content.Intent)
	 */
	@Override
	public IBinder onBind(Intent arg0) {
		// TODO Auto-generated method stub
		return null;
	}
	
	@Override 
	public void onCreate(){
		Log.i("musikcube::Service","CREATE");
		this.library	= org.musikcube.core.Library.GetInstance();
	}

	/* (non-Javadoc)
	 * @see android.app.Service#onStart(android.content.Intent, int)
	 */
	@Override
	public void onStart(Intent intent, int startId) {
		// TODO Auto-generated method stub
		super.onStart(intent, startId);

		if(intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist")!=null){
			
			this.nowPlaying			= intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist");
			this.nowPlayingPosition	= intent.getIntExtra("org.musikcube.Service.position", 0);
			
			Log.i("musikcube::Service","onStart "+this.nowPlaying.size());
			
			if(this.player==null){
				this.player	= new MediaPlayer();
			}
			
			Log.i("musikcube::Service","onStart2 "+(this.player!=null));
			this.library.WaitForAuthroization();
			
			try {
				Log.i("musikcube::Service","onStart3 "+"http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+this.nowPlaying.get(this.nowPlayingPosition)+"&auth_key="+this.library.authorization);
				this.player.setDataSource("http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+this.nowPlaying.get(this.nowPlayingPosition)+"&auth_key="+this.library.authorization);
				Log.i("musikcube::Service","onStart4");
				this.player.prepare();
				Log.i("musikcube::Service","onStart5");
				this.player.start();
				Log.i("musikcube::Service","onStart6");
				
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}

	

}
