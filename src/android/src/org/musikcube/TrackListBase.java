/**
 * 
 */
package org.musikcube;

import org.musikcube.core.IQuery;
import org.musikcube.core.Library;
import org.musikcube.core.MetadataQuery;
import org.musikcube.core.Track;
import org.musikcube.core.IQuery.OnQueryResultListener;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

/**
 * @author doy
 *
 */

public class TrackListBase extends ListActivity implements OnQueryResultListener {
	
	public java.util.ArrayList<Integer> trackList	= new java.util.ArrayList<Integer>();	
	public java.util.TreeMap<Integer, Track> trackCache	= new java.util.TreeMap<Integer,Track>();	
	public java.util.TreeSet<Integer> waitingTracks = new java.util.TreeSet<Integer>();	
	
	protected int trackListViewId		= R.layout.track_list;
	protected int trackListItemViewId	= R.layout.track_list_item;
	
	protected java.lang.Object lock 	= new java.lang.Object();
	
	protected int position		= -1;
	protected boolean markPosition	= false;
	
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
		ImageView marker;
	}
	
    public class ResultAdapter extends BaseAdapter{

    	protected TrackListBase trackList;
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
				view	= inflator.inflate(TrackListBase.this.trackListItemViewId, null);
				
				holder	= new TrackViewHolder();
				holder.title	= (TextView) view.findViewById(R.id.title); 
				holder.track	= (TextView) view.findViewById(R.id.track); 
				holder.artist	= (TextView) view.findViewById(R.id.artist); 
				if(TrackListBase.this.markPosition){
					holder.marker	= (ImageView) view.findViewById(R.id.PlayingImage); 
				}
				view.setTag(holder);
				
			}else{
				holder	= (TrackViewHolder)view.getTag();
			}
			
			if(holder.marker!=null){
				if(position==this.trackList.position){
					holder.marker.setImageResource(R.drawable.ic_playing);
				}else{
					holder.marker.setImageBitmap(null);
				}
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
  
    
    protected ResultAdapter listAdapter;
    
	@Override
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		//Log.v("musikcube.TrackList", "start");
		this.setContentView(this.trackListViewId);
		
		this.listAdapter	= new ResultAdapter(this);
		this.listAdapter.trackList = this;
		setListAdapter(this.listAdapter);

		this.registerForContextMenu(this.getListView());
	}
	
	public void OnResults(){
		this.listAdapter.notifyDataSetChanged();
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
		this.trackCache.clear();
	}

	@Override
	protected void onResume() {
		super.onResume();
		startService(new Intent(this, org.musikcube.Service.class));
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
    	if(Helper.DefaultOptionsItemSelected(item,this)){
    		return true;
    	}else{
    		return super.onContextItemSelected(item);
    	}
   	}
	
}
