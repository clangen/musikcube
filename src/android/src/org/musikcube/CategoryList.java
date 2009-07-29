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
public class CategoryList extends ListActivity implements OnQueryResultListener {
	
	private String category	= null;
	private String nextCategoryList	= "";
	private ListQuery query	= new ListQuery();
	
	private ArrayList<String> selectedCategory; 
	private ArrayList<Integer> selectedCategoryIds; 
	
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
            mTitle.setTextSize(22);
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
		

		// Extract the category order
		String categoryString	= intent.getStringExtra("org.musikcube.CategoryList.listCategory");
		String[] categories	= categoryString.split(",");
		this.category	= categories[0];
		
		// Save the next category lists
		for(int i=1;i<categories.length;i++){
			if(i>1){
				this.nextCategoryList	+= ",";
			}
			this.nextCategoryList	+= categories[i];
		}

		this.setTitle("musikCube: "+this.category);
		
	
		if(this.category!=null){
			Log.v("musikcube.CategoryList", "category="+this.category);
			// Query for data
			this.query.category	= this.category;
			
			// check for selection
			this.selectedCategory 		= intent.getStringArrayListExtra("org.musikcube.CategoryList.selectedCategory");
			this.selectedCategoryIds 	= intent.getIntegerArrayListExtra("org.musikcube.CategoryList.selectedCategoryId");
			if(this.selectedCategory!=null){
				for(int i=0;i<this.selectedCategory.size();i++){
					this.query.SelectData(this.selectedCategory.get(i), this.selectedCategoryIds.get(i));
				}
			}
			
			org.musikcube.core.Library library	= org.musikcube.core.Library.GetInstance();
			
			library.AddQuery(this.query);
			
		}else{
			Log.v("musikcube.CategoryList", "category=null");
			
		}

		Log.v("musikcube.CategoryList", "onCreate end");
		
	}
	
	public void OnResults(){
		Log.i("CategoryList::OnResults","In right thread "+this.query.resultsStrings.size());
		this.listAdapter.notifyDataSetChanged();
	}

	public void OnQueryResults() {
		// Call in right thread
		this.callbackHandler.post(this.callbackRunnable);
	}
	
	@SuppressWarnings("unchecked")
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id){
		Log.i("CategoryList::onListItemClick","clicked on "+position+" "+id);
		
		// List category
		if(this.selectedCategory==null){
			this.selectedCategory	= new ArrayList<String>();
		}
		if(this.selectedCategoryIds==null){
			this.selectedCategoryIds	= new ArrayList<Integer>();
		}
		ArrayList<String> selectedCategory	= (ArrayList<String>)this.selectedCategory.clone(); 
		ArrayList<Integer> selectedCategoryIds	= (ArrayList<Integer>)this.selectedCategoryIds.clone(); 

		selectedCategory.add(this.category);
		selectedCategoryIds.add((int)id);
		
		if(this.nextCategoryList.equals("")){
			// List tracks
			Intent intent	= new Intent(this, TrackList.class);
			intent.putExtra("org.musikcube.CategoryList.listCategory", this.nextCategoryList);
			intent.putExtra("org.musikcube.CategoryList.selectedCategory", selectedCategory);
			intent.putExtra("org.musikcube.CategoryList.selectedCategoryId", selectedCategoryIds);
			startActivity(intent);
		}else{
			Intent intent	= new Intent(this, CategoryList.class);
			intent.putExtra("org.musikcube.CategoryList.listCategory", this.nextCategoryList);
			intent.putExtra("org.musikcube.CategoryList.selectedCategory", selectedCategory);
			intent.putExtra("org.musikcube.CategoryList.selectedCategoryId", selectedCategoryIds);
			startActivity(intent);
		}
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
