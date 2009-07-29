package org.musikcube;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;

import org.musikcube.CategoryList;


public class main extends Activity {
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        ImageButton genreButton	= (ImageButton)findViewById(R.id.GenresButton);
        genreButton.setOnClickListener(this.onGenreClick);
        
        ImageButton artistsButton	= (ImageButton)findViewById(R.id.ArtistsButton);
        artistsButton.setOnClickListener(this.onArtistsClick);
        
    }
    
    private OnClickListener onGenreClick = new OnClickListener() {
    	
    	public void onClick(View v){
    		Intent intent	= new Intent(main.this, CategoryList.class);
    		intent.putExtra("org.musikcube.CategoryList.listCategory", "genre,artist,album");
    		startActivity(intent);
  		
    	}

    };
    
    private OnClickListener onArtistsClick = new OnClickListener() {
    	
    	public void onClick(View v){
    		Intent intent	= new Intent(main.this, CategoryList.class);
    		intent.putExtra("org.musikcube.CategoryList.listCategory", "artist,album");
    		startActivity(intent);
  		
    	}

    };
    
    public boolean onCreateOptionsMenu(Menu menu) {
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.default_menu, menu);
        return true;
    }    
    
    public boolean onOptionsItemSelected(MenuItem item) {
    	Log.i("MC2.onContextItemSelected","item "+item.getItemId()+" "+R.id.context_settings);
   	  switch (item.getItemId()) {
    	  case R.id.context_settings:
    		  	Intent intent	= new Intent(main.this, Preferences.class);
	      		startActivity(intent);
    		  return true;
    	  default:
    		  return super.onContextItemSelected(item);
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