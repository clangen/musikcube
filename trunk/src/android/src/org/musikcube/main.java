package org.musikcube;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
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
    		intent.putExtra("org.musikcube.CategoryList.listCategory", "genre");
    		startActivity(intent);
  		
    	}

    };
    
    private OnClickListener onArtistsClick = new OnClickListener() {
    	
    	public void onClick(View v){
    		Intent intent	= new Intent(main.this, CategoryList.class);
    		intent.putExtra("org.musikcube.CategoryList.listCategory", "artist");
    		startActivity(intent);
  		
    	}

    };
    
}