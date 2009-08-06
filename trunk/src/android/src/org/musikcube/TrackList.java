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
    
    public class ResultAdapter extends BaseAdapter{

    	protected TrackList trackList;
    	protected Context context;
    	
    	public ResultAdapter(Context context){
    		this.context	= context;
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
			TrackItemView item;
			if(view==null){
				item = new TrackItemView(this.context,this.trackList.GetTrack(position));
			}else{
				item	= (TrackItemView)view;
				item.SetTitles(this.trackList.GetTrack(position));
				
			}
			return item;
		}
    	
    }
    
    private class TrackItemView extends LinearLayout {
        public TrackItemView(Context context, Track track) {
            super(context);
            this.setOrientation(VERTICAL);

            mTitle = new TextView(context);
            mTitle.setTextSize(22);
            this.SetTitles(track);
            addView(mTitle, new LinearLayout.LayoutParams(
                    LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));

        }

        /**
         * Convenience method to set the title of a CategoryItemView
         */
        public void SetTitles(Track track) {
        	if(track!=null){
        		String text	= "";
        		String trackNumber	= track.metadata.get("track");
        		String title	= track.metadata.get("title");
        		if(trackNumber!=null){
        			text	+= trackNumber+". ";
        		}
        		if(title!=null){
        			text	+= title;
        		}
        		if(!text.equals("")){
        			mTitle.setText(text);
        		}else{
        			mTitle.setText("unknown");
        		}
        	}else{
                mTitle.setText("loading");        		
        	}
        }

        private TextView mTitle;
    }
    
    private ResultAdapter listAdapter;
    
	@Override
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		Log.v("musikcube.TrackList", "start");
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
	
}
