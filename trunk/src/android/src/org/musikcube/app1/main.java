package org.musikcube.app1;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Handler;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

import org.musikcube.core.Library;
import org.musikcube.core.Library.OnLibraryStatusListener;


public class main extends Activity implements OnLibraryStatusListener {
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        Button genreButton	= (Button)findViewById(R.id.GenresButton);
        genreButton.setOnClickListener(this.onGenreClick);
        genreButton.setEnabled(false);
        
        Button artistsButton	= (Button)findViewById(R.id.ArtistsButton);
        artistsButton.setOnClickListener(this.onArtistsClick);
        artistsButton.setEnabled(false);
        
        Button yearButton	= (Button)findViewById(R.id.YearButton);
        yearButton.setOnClickListener(this.onYearClick);
        yearButton.setEnabled(false);
        
        Button bpmGenreButton	= (Button)findViewById(R.id.BPMGenreButton);
        bpmGenreButton.setOnClickListener(this.onBPMGenreClick);
        bpmGenreButton.setEnabled(false);
        Button bpmArtistButton	= (Button)findViewById(R.id.BPMArtistButton);
        bpmArtistButton.setOnClickListener(this.onBPMArtistClick);
        bpmArtistButton.setEnabled(false);
                
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

    private OnClickListener onYearClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(main.this, CategoryList.class);
    		intent.putExtra("org.musikcube.CategoryList.listCategory", "year,artist,album");
    		startActivity(intent);
    	}
    };
    
    private OnClickListener onBPMGenreClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(main.this, CategorySelect.class);
    		intent.putExtra("org.musikcube.CategorySelect.listCategory", "genre");
    		startActivity(intent);
    	}
    };
    
    private OnClickListener onBPMArtistClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(main.this, CategorySelect.class);
    		intent.putExtra("org.musikcube.CategorySelect.listCategory", "artist");
    		startActivity(intent);
    	}
    };
    
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
    
	@Override
	protected void onPause() {
//		Log.v("mC2::Main","onPause");
		super.onPause();
		org.musikcube.core.Library.GetInstance().RemovePointer();
		org.musikcube.core.Library.GetInstance().SetStatusListener(null);
	}

	@Override
	protected void onResume() {
		super.onResume();
		startService(new Intent(this, org.musikcube.app1.Service.class));
		org.musikcube.core.Library.GetInstance().AddPointer();
		org.musikcube.core.Library.GetInstance().SetStatusListener(this);
	}

	public void OnLibraryStatusChange(int status) {
		this.libStatusHandler.post(this.libStatusRunnable);
	}
	
    final Handler libStatusHandler = new Handler();
    
    // Create runnable for posting
    final Runnable libStatusRunnable = new Runnable() {
        public void run() {
            OnLibraryStatus();
        }
    };
    
	public void OnLibraryStatus() {
		int status	= Library.GetInstance().GetStatus();
        Button genreButton	= (Button)findViewById(R.id.GenresButton);
        Button artistsButton	= (Button)findViewById(R.id.ArtistsButton);
        Button yearButton	= (Button)findViewById(R.id.YearButton);
        Button bpmGenreButton	= (Button)findViewById(R.id.BPMGenreButton);
        Button bpmArtistButton	= (Button)findViewById(R.id.BPMArtistButton);

        if(status==Library.STATUS_CONNECTED){
            genreButton.setEnabled(true);
            artistsButton.setEnabled(true);
            yearButton.setEnabled(true);
            bpmGenreButton.setEnabled(true);
            bpmArtistButton.setEnabled(true);
        }else{
            genreButton.setEnabled(false);
            artistsButton.setEnabled(false);
            yearButton.setEnabled(false);
            bpmGenreButton.setEnabled(false);
            bpmArtistButton.setEnabled(false);
        }
        
        TextView statusText	= (TextView)findViewById(R.id.StatusView);
        
        switch(status){
        case Library.STATUS_AUTHENTICATING:
        	statusText.setText("Status: Authenticating");
        	break;
        case Library.STATUS_CONNECTING:
        	statusText.setText("Status: Connecting");
        	break;
        case Library.STATUS_SHUTDOWN:
        	statusText.setText("Status: Disconnected");
        	break;
        case Library.STATUS_CONNECTED:
        	statusText.setText("Status: Connected");
        	break;
        case Library.STATUS_ERROR:
        	statusText.setText("Status: Error connecting");
        	break;
        }
        
		
	}
    
    
}