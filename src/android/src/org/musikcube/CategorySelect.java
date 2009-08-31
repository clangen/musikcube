/**
 * 
 */
package org.musikcube;

import java.util.ArrayList;

import org.musikcube.core.IQuery;
import org.musikcube.core.ListQuery;
import org.musikcube.core.Workout;
import org.musikcube.core.IQuery.OnQueryResultListener;

import android.app.ListActivity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.util.SparseBooleanArray;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.CheckedTextView;
import android.widget.ListView;

/**
 * @author doy
 *
 */
public class CategorySelect extends ListActivity implements OnQueryResultListener {
	
	private String category	= null;
	private String nextCategoryList	= "";
	private ListQuery query	= new ListQuery();
	
	private ArrayList<String> selectedCategory; 
	private ArrayList<Integer> selectedCategoryIds; 
//	private ProgressDialog loadingDialog;
	
	// Need handler for callbacks to the UI thread
    final Handler callbackHandler = new Handler();

    // Create runnable for posting
    final Runnable callbackRunnable = new Runnable() {
        public void run() {
            OnResults();
        }
    };

    static class CategoryViewHolder{
    	CheckedTextView title;
	}
    
    public class ResultAdapter extends BaseAdapter{

    	protected ListQuery query;
    	protected ListActivity context;
    	private LayoutInflater inflator;
    	
    	public ResultAdapter(ListActivity context){
    		this.context	= context;
    		this.inflator	= context.getLayoutInflater();
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
			final CategoryViewHolder holder;
			if(view==null){
				view	= inflator.inflate(R.layout.category_select_item, null);
				holder	= new CategoryViewHolder();
				holder.title	= (CheckedTextView) view.findViewById(R.id.text); 
				view.setTag(holder);
			}else{
				holder	= (CategoryViewHolder)view.getTag();
			}

			//
   			holder.title.setText(this.query.resultsStrings.get(position));
			return view;
		}
    	
    }
    
    private OnClickListener onSaveClick = new OnClickListener() {
    	public void onClick(View v){
    		final Intent intent	= new Intent(CategorySelect.this, PlayerBPMControl.class);
    		
    		// Get selection
    		final SparseBooleanArray selectionList	= CategorySelect.this.getListView().getCheckedItemPositions(); 
    		final ArrayList<Integer> selections	= new ArrayList<Integer>();
    		ArrayList<Integer> list	= CategorySelect.this.query.resultsInts;
    		int listSize	= list.size();
    		for(int i=0;i<listSize;i++){
    			if(selectionList.get(i)){
    				selections.add(list.get(i));
    			}
    		}
    		Workout.GetInstance().SetCategory(selections, CategorySelect.this.category);
    		Log.v("mC2::SAVE",CategorySelect.this.category);    		
    		Log.v("mC2::SAVE",selections.toString());    		
    		intent.putExtra("org.musikcube.PlayerBPMControl.category", CategorySelect.this.category);
    		intent.putExtra("org.musikcube.PlayerBPMControl.selection", selections);
    		startActivity(intent);
    	}
    };
    
    
    private ResultAdapter listAdapter;
    
	@Override
	protected void onCreate(Bundle savedInstanceState){
		super.onCreate(savedInstanceState);
		//Log.v("musikcube.CategoryList", "start");
		this.setContentView(R.layout.category_select);
		
		final ListView list	= this.getListView(); 
		list.setItemsCanFocus(false);
		list.setChoiceMode(ListView.CHOICE_MODE_MULTIPLE);
		View footer	= this.getLayoutInflater().inflate(R.layout.category_select_footer, null);
		list.addFooterView(footer);
		
        Button saveButton	= (Button)findViewById(R.id.SaveButton);
        saveButton.setOnClickListener(this.onSaveClick);
		
		Intent intent	= this.getIntent();
		
		this.query.SetResultListener(this);
		
		this.listAdapter	= new ResultAdapter(this);
		this.listAdapter.query	= this.query;
		setListAdapter(this.listAdapter);
		

		// Extract the category order
		String categoryString	= intent.getStringExtra("org.musikcube.CategorySelect.listCategory");
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
			//Log.v("musikcube.CategoryList", "category="+this.category);
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
			
			//this.loadingDialog = ProgressDialog.show(this, "", "Loading "+this.category+"...", true);
            library.AddQuery(this.query);
			
		}else{
			//Log.v("musikcube.CategoryList", "category=null");
			
		}

	}
	
	public void OnResults(){
		//Log.i("CategoryList::OnResults","In right thread "+this.query.resultsStrings.size());
		/*if(this.loadingDialog!=null){
			this.loadingDialog.dismiss();
			this.loadingDialog	= null;
		}*/
		this.listAdapter.notifyDataSetChanged();
	}

	public void OnQueryResults(IQuery query) {
		// Call in right thread
		this.callbackHandler.post(this.callbackRunnable);
	}
	
	/*	
	@SuppressWarnings("unchecked")
	@Override
	protected void onListItemClick(ListView l, View v, int position, long id){
		//Log.i("CategoryList::onListItemClick","clicked on "+position+" "+id);
		
		// List category
		if(this.selectedCategory==null){
			this.selectedCategory	= new ArrayList<String>();
		}
		if(this.selectedCategoryIds==null){
			this.selectedCategoryIds	= new ArrayList<Integer>();
		}
		ArrayList<String> selectedCategory	= (ArrayList<String>)this.selectedCategory.clone(); 
		ArrayList<Integer> selectedCategoryIds	= (ArrayList<Integer>)this.selectedCategoryIds.clone(); 

		if(id!=0){
			selectedCategory.add(this.category);
			selectedCategoryIds.add((int)id);
		}
		
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
	}*/

	@Override
	protected void onPause() {
		super.onPause();
		org.musikcube.core.Library.GetInstance().RemovePointer();
	}

	@Override
	protected void onResume() {
		super.onResume();
		startService(new Intent(this, org.musikcube.Service.class));
		org.musikcube.core.Library.GetInstance().AddPointer();
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
