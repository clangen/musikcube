package org.musikcube;

import org.musikcube.core.Workout;

import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.view.MenuItem;

public class Helper {

	
    public static boolean DefaultOptionsItemSelected(MenuItem item,Context context) {
    	//Log.i("MC2.onContextItemSelected","item "+item.getItemId()+" "+R.id.context_settings);
   	  switch (item.getItemId()) {
		  case R.id.context_settings:
			  context.startActivity(new Intent(context, org.musikcube.Preferences.class));
			  return true;
		  case R.id.context_browse:
			  context.startActivity(new Intent(context, org.musikcube.main.class));
			  return true;
		  case R.id.context_controls:
			  if(Workout.GetInstance().Active()){
				  context.startActivity(new Intent(context, org.musikcube.PlayerBPMControl.class));
			  }else{
				  context.startActivity(new Intent(context, org.musikcube.PlayerControl.class));
			  }
			  return true;
		  case R.id.context_nowplaying:
			  context.startActivity(new Intent(context, org.musikcube.NowPlayingList.class));
			  return true;
		  case R.id.context_help:
			  Dialog dialog = new Dialog(context);
			  dialog.setContentView(R.layout.help);
			  dialog.setTitle("Help");
			  
			  dialog.show();
			  return true;
    	  default:
    		  return false;
    	  }
   	}
	
}
