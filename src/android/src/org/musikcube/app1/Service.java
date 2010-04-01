/**
 * 
 */
package org.musikcube.app1;

import org.musikcube.core.Library;
import org.musikcube.core.Player;
import org.musikcube.core.Track;
import org.musikcube.core.Workout;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;

/**
 * @author doy
 *
 */
public class Service extends android.app.Service {

	Library library;
	Player player;
	boolean showingNotification	= false;
	boolean bpmMode	= false;
	Workout workout;
	
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
		this.player	= Player.GetInstance();
		this.player.service	= this;
		this.library	= org.musikcube.core.Library.GetInstance();
		this.library.Startup(this);
		
		TelephonyManager telephony = (TelephonyManager)this.getSystemService(Context.TELEPHONY_SERVICE);
		telephony.listen(phoneStateListener,PhoneStateListener.LISTEN_CALL_STATE);		
	}
	
    private PhoneStateListener phoneStateListener = new PhoneStateListener() {
    	public boolean callPause	= false;
    	public void onCallStateChanged(int state,String incomingNumber){
    	    switch(state)
    	    {
    	     case TelephonyManager.CALL_STATE_OFFHOOK:
    	     case TelephonyManager.CALL_STATE_RINGING:
    	    	 if(Player.GetInstance().Playing()){
	    	    	 callPause	= true;
	    	    	 Intent intent	= new Intent(Service.this, org.musikcube.app1.Service.class);
	    	    	 intent.putExtra("org.musikcube.Service.action", "pause");
	    	    	 startService(intent);
    	    	 }
    	     break;
    	     case TelephonyManager.CALL_STATE_IDLE:
    	    	 if(callPause && Player.GetInstance().Paused()){
	    	    	 Intent intent	= new Intent(Service.this, org.musikcube.app1.Service.class);
	    	    	 intent.putExtra("org.musikcube.Service.action", "pause");
	    	    	 startService(intent);
    	    	 }
    	    }
    	}
    };
	

	/* (non-Javadoc)
	 * @see android.app.Service#onStart(android.content.Intent, int)
	 */
	@Override
	public void onStart(Intent intent, int startId) {
		// TODO Auto-generated method stub
		super.onStart(intent, startId);
		
		
		boolean stopWorkout	= false;
		
		String action	= intent.getStringExtra("org.musikcube.Service.action");
		if(action==null){
			this.library.Startup(this);
			return;
		}
		if(action.equals("playlist")){
			Player player	= Player.GetInstance();
			player.Play(intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist"), intent.getIntExtra("org.musikcube.Service.position", 0));
			stopWorkout	= true;
		}
		if(action.equals("appendlist")){
			Player player	= Player.GetInstance();
			player.Append(intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist"));
			stopWorkout	= true;
		}
		if(action.equals("remove_from_list")){
			Player player	= Player.GetInstance();
			player.RemoveFromList(intent.getIntExtra("org.musikcube.Service.position", -1));
		}
		
		if(action.equals("playlist_prepare")){
			Player player	= Player.GetInstance();
			player.PlayWhenPrepared(intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist"), intent.getIntExtra("org.musikcube.Service.position", 0));
		}
				
		if(action.equals("next")){
			Player player	= Player.GetInstance();
			player.Next();
		}
		if(action.equals("prev")){
			Player player	= Player.GetInstance();
			player.Prev();
		}
		if(action.equals("stop")){
			Player player	= Player.GetInstance();
			player.Stop();
			stopWorkout	= true;
		}
		if(action.equals("pause")){
			Player player	= Player.GetInstance();
			player.Pause();
			stopWorkout	= true;
		}
		if(action.equals("play")){
			Player player	= Player.GetInstance();
			player.Play();
		}
		if(action.equals("shutdown")){
			//Log.i("musikcube::Service","Shutdown");
			this.stopSelf();
			stopWorkout	= true;
		}
		
		if(stopWorkout){
			if(this.workout!=null){
				this.workout.Stop();
				this.workout	= null;
			}
		}
		
		if(action.equals("workoutstart")){
			this.workout	= Workout.GetInstance();
			this.workout.Startup(this);
		}
		if(action.equals("workoutstop")){
			if(this.workout!=null){
				this.workout.Stop();
				this.workout	= null;
			}
			Player player	= Player.GetInstance();
			player.Stop();
		}
				
		if(action.equals("player_start") || this.workout!=null){
			Track track	= Player.GetInstance().GetCurrentTrack();
			
			this.showingNotification	= true;
			
			String ns = Context.NOTIFICATION_SERVICE;
			NotificationManager mNotificationManager = (NotificationManager) getSystemService(ns);			
			int icon = R.drawable.mc_notify;
			CharSequence tickerText = "mC2 playing";
			long when = System.currentTimeMillis();
			Notification notification	= new Notification(icon, tickerText, when);
			Context context = getApplicationContext();
			String contentTitle = "Playing: ";
			if(track!=null){
				String trackTitle	= track.metadata.get("title");
				if(trackTitle!=null){
					contentTitle	+= trackTitle;
				}
			}
			
			String contentText = "By: ";
			if(track!=null){
				String trackArtist	= track.metadata.get("visual_artist");
				if(trackArtist!=null){
					contentText 	+= trackArtist;
				}
			}
			Intent notificationIntent;
			if(this.workout==null){
				notificationIntent = new Intent(this, PlayerControl.class);
			}else{
				notificationIntent = new Intent(this, PlayerBPMControl.class);
			}
			
			PendingIntent contentIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);
			
			notification.flags	|= Notification.FLAG_ONGOING_EVENT|Notification.FLAG_NO_CLEAR;

			notification.setLatestEventInfo(context, contentTitle, contentText, contentIntent);		
			mNotificationManager.notify(1, notification);		
		}
		
		if(action.equals("player_end") && this.workout==null){
			this.showingNotification	= false;
			String ns = Context.NOTIFICATION_SERVICE;
			NotificationManager mNotificationManager = (NotificationManager) getSystemService(ns);
			mNotificationManager.cancel(1);
		}
						
//		if(intent getIntegerArrayListExtra("org.musikcube.Service.tracklist")!=null){
		
/*		if(intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist")!=null){
			
			this.nowPlaying			= intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist");
			this.nowPlayingPosition	= intent.getIntExtra("org.musikcube.Service.position", 0);
			
			Log.i("musikcube::Service","onStart "+this.nowPlaying.size());
			
			if(this.player==null){
				this.player	= new MediaPlayer();
			}
			
			Log.i("musikcube::Service","onStart2 "+(this.player!=null));
			this.library.WaitForAuthroization();
			
			try {
//				Log.i("musikcube::Service","onStart3 "+"http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+this.nowPlaying.get(this.nowPlayingPosition)+"&auth_key="+this.library.authorization);
				this.player.setDataSource("http://"+this.library.host+":"+this.library.httpPort+"/track/?track_id="+this.nowPlaying.get(this.nowPlayingPosition)+"&auth_key="+this.library.authorization);
				Log.i("musikcube::Service","onStart4");
				this.player.prepare();
				Log.i("musikcube::Service","onStart5");
				this.player.start();
				Log.i("musikcube::Service","onStart6");
				
			} catch (Exception e) {
				e.printStackTrace();
			}
		}*/
	}

	@Override
	public void onDestroy() {
		//Log.i("musikcube::Service","EXIT");
		this.library.Exit();
		super.onDestroy();
	}

	

}
