package org.musikcube;

import android.app.Activity;
import android.os.Bundle;
import org.musikcube.core.Library;

public class main extends Activity {
	
	private org.musikcube.core.Library library;
	
    /** Called when the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        
        this.library	= new org.musikcube.core.Library();
        this.library.Connect("192.168.99.100", "doep", "doep", 10543, 10544);
        
        
    }
}