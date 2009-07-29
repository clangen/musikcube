/**
 * 
 */
package org.musikcube;

import java.util.ArrayList;

import org.musikcube.core.ListQuery;
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
	
	// Need handler for callbacks to the UI thread
    final Handler callbackHandler = new Handler();

    // Create runnable for posting
    final Runnable callbackRunnable = new Runnable() {
        public void run() {
            OnResults();
        }
    };
    
    public class ResultAdapter extends BaseAdapter{

    	protected ListQuery query;
    	protected Context context;
    	
    	public ResultAdapter(Context context){
    		this.context	= context;
    	}
    	
		public int getCount() {
			return this.query.trackList.size();
		}

		public Object getItem(int position) {
			return this.query.trackList.get(position);
		}

		public long getItemId(int position) {
			return this.query.trackList.get(position);
		}

		public View getView(int position, View view, ViewGroup parent) {
			TrackItemView item;
			if(view==null){
				item = new TrackItemView(this.context,this.query.trackList.get(position));
			}else{
				item	= (TrackItemView)view;
				item.SetTitle(this.query.trackList.get(position));
				
			}
			return item;
		}
    	
    }
    
    private class TrackItemView extends LinearLayout {
        public TrackItemView(Context context, Integer title) {
            super(context);
            this.setOrientation(VERTICAL);

            mTitle = new TextView(context);
            mTitle.setTextSize(22);
            mTitle.setText(title.toString());
            addView(mTitle, new LinearLayout.LayoutParams(
                    LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));

        }

        /**
         * Convenience method to set the title of a CategoryItemView
         */
        public void SetTitle(Integer title) {
            mTitle.setText(title.toString());
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
		this.listAdapter.query	= this.query;
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
		Log.i("TrackList::OnResults","In right thread "+this.query.trackList.size());
		this.listAdapter.notifyDataSetChanged();
	}

	public void OnQueryResults() {
		// Call in right thread
		this.callbackHandler.post(this.callbackRunnable);
	}
	
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id){
/*		Log.i("CategoryList::onListItemClick","clicked on "+position+" "+id);
		if(this.nextCategoryList.equals("")){
			// List tracks
		}else{
			Intent intent	= new Intent(this, CategoryList.class);
			intent.putExtra("org.musikcube.CategoryList.listCategory", this.nextCategoryList);
			intent.putExtra("org.musikcube.CategoryList.selectedCategory", this.category);
			intent.putExtra("org.musikcube.CategoryList.selectedCategoryId", (int)id);
			startActivity(intent);
		}*/
		
		Intent intent	= new Intent(this, org.musikcube.Service.class);
		intent.putExtra("org.musikcube.Service.tracklist", this.query.trackList);
		intent.putExtra("org.musikcube.Service.position", position);
		intent.putExtra("org.musikcube.Service.action", "playlist");
		startService(intent);
		
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
	
}
