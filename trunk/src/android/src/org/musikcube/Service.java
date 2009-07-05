/**
 * 
 */
package org.musikcube;

import android.content.Intent;
import android.os.IBinder;

/**
 * @author doy
 *
 */
public class Service extends android.app.Service {

	
	/**
	 * 
	 */
	public Service() {
		// TODO Auto-generated constructor stub
	}

	/* (non-Javadoc)
	 * @see android.app.Service#onBind(android.content.Intent)
	 */
	@Override
	public IBinder onBind(Intent arg0) {
		// TODO Auto-generated method stub
		return null;
	}
	
	@Override 
	public void onCreate(){
		org.musikcube.core.Library library	= org.musikcube.core.Library.GetInstance();
	}


}
