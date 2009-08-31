package org.musikcube.core;

import java.util.ArrayList;

import org.musikcube.core.IQuery.OnQueryResultListener;
import org.musikcube.core.PaceDetector.OnBPMListener;

import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class Workout implements OnBPMListener, Runnable, OnQueryResultListener {

	
	private static org.musikcube.core.Workout workout = null;
	private PaceDetector paceDetector;
	private Context context;
	private Object lock		= new Object();
	private float bpm	= 100;
	private Thread	thread;
	private boolean active	= false;
	private boolean useAccelerometer	= false;
	private long lastQueryTime		 = -30000;
	private int minimumPlaytime		= 30000;
	static final public float BPM_THRESHOLD	= 8;	// if BPM is off by more than 10 bpm, switch track
	
	private float reportBPM	= 100;
	
	private ArrayList<Integer> selectedCategories	= new ArrayList<Integer>(); 
	private String category	= "";
	
	public static final synchronized Workout GetInstance(){
		if(Workout.workout==null){
			Workout.workout = new org.musikcube.core.Workout();
		}
		return Workout.workout;
	}
	
	protected Workout(){
		this.bpm	= 100+(float)Math.random();
	}
	
	public void Startup(Context context){
		synchronized (lock) {
			this.context	= context;
			if(this.active==false){
				org.musikcube.core.Library.GetInstance().AddPointer();
			}
			this.active	= true;
			if(this.listener!=null){
				this.listener.OnBPMUpdate();
			}
			this.UseAccelerometer(this.useAccelerometer);
			this.QueryTracks(true);
		}
	}
	
	public void Stop(){
		synchronized (lock) {
			if(this.active==true){
				org.musikcube.core.Library.GetInstance().RemovePointer();
			}
			this.active	= false;
			this.UseAccelerometer(this.useAccelerometer);
			if(this.listener!=null){
				this.listener.OnBPMUpdate();
			}
		}
	}
	
	public void SetCategory(ArrayList<Integer> selectedCategories,String category){
		synchronized (lock) {
			this.selectedCategories	= selectedCategories;
			this.category	= category;
		}
	}
	
	public boolean Active(){
		synchronized (lock) {
			return this.active;
		}
	}
	
	
	public void UseAccelerometer(boolean use){
		synchronized(lock){
			this.useAccelerometer	= use;
			if(this.context!=null){
				if(use && this.active){
					if(this.paceDetector==null){
						this.paceDetector	= new PaceDetector();
						this.paceDetector.SetListener(this);
						this.paceDetector.StartSensor(this.context);
					}
				}
				if(!use || !this.active){
					if(this.paceDetector!=null){
						this.paceDetector.StopSensor(this.context);
						this.paceDetector	= null;
					}
				}
			}
		}
	}
	public boolean Accelerometer(){
		synchronized(lock){
			return this.useAccelerometer;
		}
	}
	
	public void SetBPM(float bpm){
		synchronized(lock){
			if(this.paceDetector==null){
				this.reportBPM	= bpm;
				this.bpm	= bpm;
				if(this.active){
					this.QueryTracks(true);
				}
			}
		}
	}
	
	public float GetBPM(){
		synchronized(lock){
			return this.reportBPM;
		}
	}

	public void OnBPMUpdate() {
		synchronized(lock){
			final float bpm	= this.paceDetector.GetBPM();
			if(bpm>0.0){
				this.reportBPM	= bpm;
//				Log.v("BPM","bpm="+bpm);
				if(bpm>this.bpm+BPM_THRESHOLD || bpm<this.bpm-BPM_THRESHOLD){
					this.bpm	= bpm;
					this.reportBPM	= bpm;
					this.QueryTracks(false);
				}
				
				if(this.listener!=null){
					this.listener.OnBPMUpdate();
				}
			}
		}
	}
	
	private void QueryTracks(boolean force){
		long currentTime	= android.os.SystemClock.elapsedRealtime();
		// Check that minimum time has passed
		synchronized(lock){
			if(currentTime>this.lastQueryTime+this.minimumPlaytime || force){
				this.lastQueryTime	= currentTime;
				// Get new tracks
				BPMQuery query	= new BPMQuery();
				query.queryForBPM	= this.bpm;
				query.selectionInts	= (ArrayList<Integer>) this.selectedCategories.clone();
				query.selectionStrings.add(this.category);
				query.SetResultListener(this);
				Library.GetInstance().AddQuery(query);
			}
		}
	}
	
	public interface OnWorkoutListener{
		public void OnBPMUpdate();
	}
	protected OnWorkoutListener listener			= null;
	
	public void SetListener(OnWorkoutListener listener){
		synchronized(this.lock){
			this.listener	= listener;
			if(this.listener!=null){
				this.listener.OnBPMUpdate();
			}
		}
	}

	public void run() {
	}

	public void OnQueryResults(IQuery query) {
		synchronized(this.lock){
			if(this.active){
				this.lastQueryTime	= android.os.SystemClock.elapsedRealtime();
				BPMQuery bpmQuery	= (BPMQuery)query;
				if(!bpmQuery.trackList.isEmpty() && this.context!=null){
					Intent intent	= new Intent(this.context, org.musikcube.Service.class);
					intent.putExtra("org.musikcube.Service.tracklist", bpmQuery.trackList);
					intent.putExtra("org.musikcube.Service.position", 0);
					intent.putExtra("org.musikcube.Service.action", "playlist_prepare");
					this.context.startService(intent);
				}
			}
		}
	}
	
}
