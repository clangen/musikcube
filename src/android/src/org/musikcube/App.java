package org.musikcube;

import android.app.Application;
import android.util.Log;

public class App extends Application {

	@Override 
	public void onCreate(){
		super.onCreate();
		Log.i("MUSIKCUBE::APP","Start");
		org.musikcube.core.Library library	= org.musikcube.core.Library.GetInstance();
		
		library.Startup(this);
		
	}
}
