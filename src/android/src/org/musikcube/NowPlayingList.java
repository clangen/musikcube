/**
 * 
 */
package org.musikcube;

import org.musikcube.core.Player;
import org.musikcube.core.Player.OnTrackListUpdateListener;

import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;
import android.view.ContextMenu.ContextMenuInfo;
import android.widget.AdapterView.AdapterContextMenuInfo;

/**
 * @author doy
 *
 */

public class NowPlayingList extends TrackListBase implements OnTrackListUpdateListener{
	
	
	final static public int PLAY_THIS_ID	= 0;
	final static public int ADD_THIS_ID	= 1;
	final static public int ADD_ALL_ID	= 2;
	final static public int REMOVE_ID	= 3;
	
	@Override
	protected void onCreate(Bundle savedInstanceState){

		this.markPosition	= true;
		
		this.trackListViewId	= R.layout.now_playing_list;
		this.trackListItemViewId	= R.layout.now_playing_list_item;
		
		super.onCreate(savedInstanceState);
		
		this.setTitle("musikCube: Now playing");
		
		this.registerForContextMenu(this.getListView());
	}
	
	
	
	@Override
	public boolean onContextItemSelected(MenuItem item) {
		AdapterContextMenuInfo info = (AdapterContextMenuInfo) item.getMenuInfo();
		java.util.ArrayList<Integer> trackList	= new java.util.ArrayList<Integer>();
		Intent intent	= new Intent(this, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.position", 0);
		switch (item.getItemId()) {
			case PLAY_THIS_ID:
				trackList.add((int)info.id);
				intent.putExtra("org.musikcube.Service.tracklist", trackList);
				intent.putExtra("org.musikcube.Service.action", "playlist");
				startService(intent);
				Intent intent2	= new Intent(this, PlayerControl.class);
				startActivity(intent2);
				return true;
			case ADD_THIS_ID:
				trackList.add((int)info.id);
				intent.putExtra("org.musikcube.Service.tracklist", trackList);
				intent.putExtra("org.musikcube.Service.action", "appendlist");
				startService(intent);
				return true;
			case ADD_ALL_ID:
				trackList.add((int)info.id);
				intent.putExtra("org.musikcube.Service.tracklist", this.trackList);
				intent.putExtra("org.musikcube.Service.action", "appendlist");
				startService(intent);
				return true;
			case REMOVE_ID:
				intent.putExtra("org.musikcube.Service.action", "remove_from_list");
				intent.putExtra("org.musikcube.Service.position", info.position);
				startService(intent);
				return true;
			default:
				return super.onContextItemSelected(item);
		}
	}

	@Override
	public void onCreateContextMenu(ContextMenu menu, View v,
			ContextMenuInfo menuInfo) {
		super.onCreateContextMenu(menu, v, menuInfo);
		menu.add(0, PLAY_THIS_ID, 0, "Play this track");
		menu.add(0, ADD_THIS_ID, 0,  "Add this to current playlist");
		menu.add(0, ADD_ALL_ID, 0,  "Add all to current playlist");
		menu.add(0, REMOVE_ID, 0,  "Remove from playlist");
	}
	
	public void OnTrackListPositionUpdate() {
		synchronized(this.lock){
			this.position	= Player.GetInstance().GetPosition();
		}
		this.callbackHandler.post(this.callbackRunnable);
	}

	public void OnTrackListUpdate() {
		synchronized(this.lock){
			this.trackCache.clear();
			this.waitingTracks.clear();
			this.trackList	= Player.GetInstance().GetTracklist();
			this.position	= Player.GetInstance().GetPosition();
		}
		this.callbackHandler.post(this.callbackRunnable);
	}


	@Override
	protected void onPause() {
		super.onPause();
		Player.GetInstance().SetListUpdateListener(null);
	}

	@Override
	protected void onResume() {
		super.onResume();
		Player.GetInstance().SetListUpdateListener(this);
		this.position	= Player.GetInstance().GetPosition();
		this.setSelection(this.position); 
	}
	
	
	
}
