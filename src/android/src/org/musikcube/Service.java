/**
 * 
 */
package org.musikcube;

import org.musikcube.core.Library;
import org.musikcube.core.Player;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;

/**
 * @author doy
 *
 */
public class Service extends android.app.Service {

	Library library;
	Player player;
	
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
		//Log.i("musikcube::Service","CREATE");
		this.player	= Player.GetInstance();
		this.player.service	= this;
		this.library	= org.musikcube.core.Library.GetInstance();
		this.library.Startup(this);
	}

	/* (non-Javadoc)
	 * @see android.app.Service#onStart(android.content.Intent, int)
	 */
	@Override
	public void onStart(Intent intent, int startId) {
		// TODO Auto-generated method stub
		super.onStart(intent, startId);

		String action	= intent.getStringExtra("org.musikcube.Service.action");
		if(action==null){
			return;
		}
		
		if(action.equals("playlist")){
			Player player	= Player.GetInstance();
			player.Play(intent.getIntegerArrayListExtra("org.musikcube.Service.tracklist"), intent.getIntExtra("org.musikcube.Service.position", 0));
		}
		if(action.equals("next")){
			Player player	= Player.GetInstance();
			player.Next();
		}
		if(action.equals("stop")){
			Player player	= Player.GetInstance();
			player.Stop();
		}
		if(action.equals("play")){
			Player player	= Player.GetInstance();
			player.Play();
		}
		if(action.equals("shutdown")){
			//Log.i("musikcube::Service","Shutdown");
			this.stopSelf();
		}
		if(action.equals("player_start")){
			String ns = Context.NOTIFICATION_SERVICE;
			NotificationManager mNotificationManager = (NotificationManager) getSystemService(ns);			
			int icon = R.drawable.mc_notify;
			CharSequence tickerText = "musikCube is playing";
			long when = System.currentTimeMillis();
			Notification notification	= new Notification(icon, tickerText, when);
			Context context = getApplicationContext();
			CharSequence contentTitle = "musikCube";
			CharSequence contentText = "is playing!";
			Intent notificationIntent = new Intent(this, PlayerControl.class);
			PendingIntent contentIntent = PendingIntent.getActivity(this, 0, notificationIntent, 0);
			
			notification.flags	|= Notification.FLAG_ONGOING_EVENT|Notification.FLAG_NO_CLEAR;

			notification.setLatestEventInfo(context, contentTitle, contentText, contentIntent);		
			mNotificationManager.notify(1, notification);		
		}
		
		if(action.equals("player_end")){
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
