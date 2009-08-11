/**
 * 
 */
package org.musikcube;

import java.util.ArrayList;

import org.musikcube.core.IQuery;
import org.musikcube.core.Library;
import org.musikcube.core.ListQuery;
import org.musikcube.core.MetadataQuery;
import org.musikcube.core.Track;
import org.musikcube.core.IQuery.OnQueryResultListener;

import android.app.ListActivity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

/**
 * @author doy
 *
 */

public class TrackList extends ListActivity implements OnQueryResultListener {
	
	
	private ListQuery query	= new ListQuery();
	public java.util.ArrayList<Integer> trackList	= new java.util.ArrayList<Integer>();	
	public java.util.TreeMap<Integer, Track> trackCache	= new java.util.TreeMap<Integer,Track>();	
	public java.util.TreeSet<Integer> waitingTracks = new java.util.TreeSet<Integer>();	
	
	private java.lang.Object lock 	= new java.lang.Object();
	
	// Need handler for callbacks to the UI thread
    final Handler callbackHandler = new Handler();

    // Create runnable for posting
    final Runnable callbackRunnable = new Runnable() {
        public void run() {
            OnResults();
        }
    };
    
	static class TrackViewHolder{
		TextView track;
		TextView title;
		TextView artist;
	}
	
    public class ResultAdapter extends BaseAdapter{

    	protected TrackList trackList;
    	protected ListActivity context;
    	private LayoutInflater inflator;
    	
    	public ResultAdapter(ListActivity context){
    		this.context	= context;
    		this.inflator	= context.getLayoutInflater();
    	}
    	
		public int getCount() {
			return this.trackList.trackList.size();
		}

		public Object getItem(int position) {
			return this.trackList.trackList.get(position);
		}

		public long getItemId(int position) {
			return this.trackList.trackList.get(position);
		}

		
		public View getView(int position, View view, ViewGroup parent) {
			TrackViewHolder holder;
			if(view==null){
				view	= inflator.inflate(R.layout.track_list_item, null);
				
				holder	= new TrackViewHolder();
				holder.title	= (TextView) view.findViewById(R.id.title); 
				holder.track	= (TextView) view.findViewById(R.id.track); 
				holder.artist	= (TextView) view.findViewById(R.id.artist); 
				view.setTag(holder);
				
			}else{
				holder	= (TrackViewHolder)view.getTag();
			}
			
			Track track	= this.trackList.GetTrack(position);
			if(track==null){
    			holder.track.setText("");
    			holder.title.setText("....");
    			holder.artist.setText("");
				return view;
			}
			
    		String trackNumber	= track.metadata.get("track");
    		String title		= track.metadata.get("title");
    		String artist		= track.metadata.get("visual_artist");
    		if(trackNumber!=null){
    			holder.track.setText(trackNumber);
    		}else{
    			holder.track.setText("");
    		}
    		if(title!=null){
    			holder.title.setText(title);
    		}else{
    			holder.title.setText("");
    		}
    		if(artist!=null){
    			holder.artist.setText(artist);
    		}else{
    			holder.artist.setText("");
    		}

			return view;
			
		}
    	
    }
  
    
    private ResultAdapter listAdapter;
    
	@Override
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		//Log.v("musikcube.TrackList", "start");
		this.setContentView(R.layout.track_list);
		
		Intent intent	= this.getIntent();
		
		this.query.SetResultListener(this);
		
		this.listAdapter	= new ResultAdapter(this);
		this.listAdapter.trackList = this;
		setListAdapter(this.listAdapter);
		
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
		
	}
	
	public void OnResults(){
		//Log.i("TrackList::OnResults","In right thread "+this.query.trackList.size());
		
		if(this.query!=null){
			this.trackList	= this.query.trackList;
			this.query		= null;
			this.listAdapter.notifyDataSetChanged();
		}else{
			//this is metadataquery results
			// notify that list should update
			this.listAdapter.notifyDataSetChanged();
		}
	}

	public void OnQueryResults(IQuery query) {
		
		if(query.type=="TrackMetadata"){
			MetadataQuery mdQuery	= (MetadataQuery)query;
			synchronized(this.lock){
				int trackCount	= mdQuery.resultTracks.size();
				for(int i=0;i<trackCount;i++){
					Track track	= mdQuery.resultTracks.get(i);
					this.waitingTracks.remove(track.id);
					this.trackCache.put(track.id,track);
				}
			}
		}
		
		// Call in right thread
		this.callbackHandler.post(this.callbackRunnable);
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id){		
		Intent intent	= new Intent(this, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.tracklist", this.trackList);
		intent.putExtra("org.musikcube.Service.position", position);
		intent.putExtra("org.musikcube.Service.action", "playlist");
		startService(intent);
		
		Intent intent2	= new Intent(this, PlayerControl.class);
		startActivity(intent2);
		
	}

	
	
	@Override
	protected void onPause() {
		super.onPause();
		org.musikcube.core.Library.GetInstance().RemovePointer();
	}

	@Override
	protected void onResume() {
		super.onResume();
		org.musikcube.core.Library.GetInstance().AddPointer();
	}

	public Track GetTrack(int position){
		synchronized(this.lock){
			Integer trackId	= this.trackList.get(position);
			Track track		= this.trackCache.get(trackId);
			if(track==null){
				//check if it's in the "requested" cache
				if(!this.waitingTracks.contains(trackId)){
					// request the track
					MetadataQuery query	= new MetadataQuery();
					query.requestedMetakeys.add("track");
					query.requestedMetakeys.add("title");
					query.requestedMetakeys.add("visual_artist");
					query.SetResultListener(this);
					
					query.requestedTracks.add(trackId);
					this.waitingTracks.add(trackId);
					
					// Add some more tracks
					int trackListSize	= this.trackList.size();
					for(int i=position;i<position+10 && i<trackListSize;i++){
						Integer nextTrackId	= this.trackList.get(i);
						if(!this.IsTrackInCache(nextTrackId)){
							query.requestedTracks.add(nextTrackId);
							this.waitingTracks.add(nextTrackId);
						}
					}
					
					Library.GetInstance().AddQuery(query);
				}
			}
			return track;
		}
	}
	
	private boolean IsTrackInCache(int trackId){
		Track track		= this.trackCache.get(trackId);
		if(track==null){
			//check if it's in the "requested" cache
			if(!this.waitingTracks.contains(trackId)){
				// request the track
				return false;
			}
		}
		return true;
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
