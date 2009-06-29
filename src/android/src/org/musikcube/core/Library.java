/**
 * 
 */
package org.musikcube.core;

import java.net.*;
import java.io.*;
import android.util.*;
import doep.xml.*;

/**
 * @author doy
 *
 */
public class Library implements Runnable{

	private String username;
	private String password;
	private String authorization;
	private String host;
	private int queryPort;
	private int httpPort;
	
	private Thread	thread;
	private boolean running;
	private Socket socket;
	
	private class WriterThreadHelper implements Runnable{
		private Thread	thread;
		private Library library;
		public WriterThreadHelper(Library library){
			this.library	= library;
			this.thread		= new Thread(this);
			this.thread.start();
		}
		
		public void run(){
			this.library.WriteThread();
		}
		
	}
	
	private WriterThreadHelper writerThreadHelper;
	  
	public Library(){
		this.running	= false;
	}
	
	public boolean Connect(String host,String username,String password,int queryPort,int httpPort){
		Log.i("Library","starting  "+host+":"+queryPort);
		if(!running){
			this.host	= host;
			this.username	= username;
			this.password	= password;
			this.queryPort	= queryPort;
			this.httpPort	= httpPort;

			// Startup thread
			this.thread	= new Thread(this);
//			this.thread.setDaemon(true);
			this.running	= true;
			this.thread.start();
			return true;
		}
		return false;
	}
	
	public void run(){
		// First try to connect
		try{
			this.socket	= new java.net.Socket(this.host,this.queryPort);
			//Log.v("Library::socket","Successfully connected to "+this.host+":"+this.queryPort);
			
			doep.xml.Reader reader	= new doep.xml.Reader(this.socket.getInputStream());
			//Log.v("Library::run","Reader started");
			// Wait for a "authentication" tag
			doep.xml.ReaderNode authNode=null;
			if( (authNode=reader.ChildNode("authentication")) != null ){
				//Log.v("Library::run","Authtag found");
				// Wait for authorization tag to end
				authNode.End();	
				//Log.v("Library::run","Authtag end");
				this.authorization	= authNode.content;
				//Log.v("Library::run","Authorization="+this.authorization);
			}
			
			// Start write thread
			this.writerThreadHelper		= new WriterThreadHelper(this);
			
		}
		catch(IOException x){
			Log.e("Library::socket","Unable to connect to "+this.host+":"+this.queryPort);
		}
		catch(Exception x){
			Log.e("Library::socket","E "+this.host+":"+this.queryPort);
		}
		
	}
	
	public void WriteThread(){
		
	}
	
}
