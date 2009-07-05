/**
 * 
 */
package org.musikcube;

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
import android.widget.TextView;

/**
 * @author doy
 *
 */
public class CategoryList extends ListActivity implements OnQueryResultListener {
	
	private String category	= "";
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
			return this.query.resultsInts.size();
		}

		public Object getItem(int position) {
			return this.query.resultsInts.get(position);
		}

		public long getItemId(int position) {
			return this.query.resultsInts.get(position);
		}

		public View getView(int position, View view, ViewGroup parent) {
			CategoryItemView item;
			if(view==null){
				item = new CategoryItemView(this.context,this.query.resultsStrings.get(position));
			}else{
				item	= (CategoryItemView)view;
				item.SetTitle(this.query.resultsStrings.get(position));
				
			}
			return item;
		}
    	
    }
    
    private class CategoryItemView extends LinearLayout {
        public CategoryItemView(Context context, String title) {
            super(context);
            this.setOrientation(VERTICAL);

            mTitle = new TextView(context);
            mTitle.setTextSize(18);
            mTitle.setText(title);
            addView(mTitle, new LinearLayout.LayoutParams(
                    LayoutParams.FILL_PARENT, LayoutParams.WRAP_CONTENT));

        }

        /**
         * Convenience method to set the title of a CategoryItemView
         */
        public void SetTitle(String title) {
            mTitle.setText(title);
        }

        private TextView mTitle;
    }
    
    private ResultAdapter listAdapter;
    
	@Override
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		Log.v("musikcube.CategoryList", "start");
		this.setContentView(R.layout.category_list);
		
		Intent intent	= this.getIntent();
		
		this.query.SetResultListener(this);
		
		this.listAdapter	= new ResultAdapter(this);
		this.listAdapter.query	= this.query;
		setListAdapter(this.listAdapter);
		
		this.category	= intent.getStringExtra("org.musikcube.CategoryList.listCategory");
		if(this.category!=null){
			Log.v("musikcube.CategoryList", "category="+this.category);
			// Query for data
			this.query.category	= this.category;
			
			org.musikcube.core.Library library	= org.musikcube.core.Library.GetInstance();
			
			library.AddQuery(this.query);
			
		}else{
			Log.v("musikcube.CategoryList", "category=null");
			
		}

		Log.v("musikcube.CategoryList", "onCreate end");
		
	}
	
	public void OnResults(){
		Log.i("CategoryList::OnResults","In right thread "+this.query.resultsStrings.size());
//		this.get
		this.listAdapter.notifyDataSetChanged();
/*		
		int first = this.getListView().getPositionForView(this.getListView().getChildAt(0));
		int last = first + this.getListView().getChildCount();
		
		Log.i("CategoryList::OnResults","VISIBLE "+first+" "+last);
	*/	
	}

	public void OnQueryResults() {
		// Call in right thread
		this.callbackHandler.post(this.callbackRunnable);
	}
	
}
