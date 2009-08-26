/**
 * 
 */
package org.musikcube;

import java.util.ArrayList;

import org.musikcube.core.ListQuery;
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

public class TrackList extends TrackListBase {
	
	
	private ListQuery query	= new ListQuery();
	
	final static public int PLAY_THIS_ID	= 0;
	final static public int ADD_THIS_ID	= 1;
	final static public int ADD_ALL_ID	= 2;
	
	@Override
	protected void onCreate(Bundle savedInstanceState){
		
		super.onCreate(savedInstanceState);
		
		Intent intent	= this.getIntent();
		
		this.query.SetResultListener(this);
		
		this.setTitle("musikCube: Tracks");
		
		// Query for data
		
		// check for selection
		ArrayList<String> selectedCategory 		= intent.getStringArrayListExtra("org.musikcube.CategoryList.selectedCategory");
		ArrayList<Integer> selectedCategoryIds 	= intent.getIntegerArrayListExtra("org.musikcube.CategoryList.selectedCategoryId");
		if(selectedCategory!=null){
			for(int i=0;i<selectedCategory.size();i++){
				this.query.SelectData(selectedCategory.get(i), selectedCategoryIds.get(i));
			}
		}
		
		org.musikcube.core.Library library	= org.musikcube.core.Library.GetInstance();
		
		this.query.listTracks	= true;
		library.AddQuery(this.query);
		
		this.registerForContextMenu(this.getListView());
	}
	
	@Override
	public void OnResults(){
		if(this.query!=null){
			this.trackList	= this.query.trackList;
			this.query		= null;
		}
		super.OnResults();
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
	}
	
}
