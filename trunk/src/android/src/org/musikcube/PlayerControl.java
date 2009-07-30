package org.musikcube;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageButton;

public class PlayerControl extends Activity {

	
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.play_control);
        
        ImageButton nextButton	= (ImageButton)findViewById(R.id.MediaNext);
        nextButton.setOnClickListener(this.onNextClick);
        ImageButton pauseButton	= (ImageButton)findViewById(R.id.MediaPause);
        pauseButton.setOnClickListener(this.onPauseClick);
        
    }

    private OnClickListener onNextClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(PlayerControl.this, org.musikcube.Service.class);
    		intent.putExtra("org.musikcube.Service.action", "next");
    		startService(intent);
    	}
    };
    private OnClickListener onPauseClick = new OnClickListener() {
    	public void onClick(View v){
    		Intent intent	= new Intent(PlayerControl.this, org.musikcube.Service.class);
    		intent.putExtra("org.musikcube.Service.action", "stop");
    		startService(intent);
    	}
    };
    
    
}
