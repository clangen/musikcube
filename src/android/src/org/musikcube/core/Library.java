/**
 * 
 */
package org.musikcube.core;

import java.net.*;
import java.io.*;
import android.util.*;
import doep.xml.*;

import org.musikcube.Service;
import org.musikcube.core.IQuery;

/**
 * @author doy
 *
 */
public class Library implements Runnable{

	private String username;
	private String password;
	public String authorization	= "";
	public String host;
	private int queryPort;
	public int httpPort;
	
	private Thread	thread;
	private boolean running	= false;
	private Socket socket;
	
	private java.lang.Object notifier	= new java.lang.Object();
	
	private java.util.LinkedList<IQuery> sendQueryQueue	= new java.util.LinkedList<IQuery>(); 
	private java.util.LinkedList<IQuery> waitingQueryQueue	= new java.util.LinkedList<IQuery>(); 
	
	
	private static org.musikcube.core.Library library = null;
	
	public static final synchronized Library GetInstance(){
		if(Library.library==null){
			Library.library	= new org.musikcube.core.Library();
			Library.library.Connect("192.168.99.100", "doep", "doep", 10543, 10544);
		}
		
		return Library.library;
	}

	
	private class WriterThreadHelper implements Runnable{
		private Thread	thread;
		private Library library;
		public WriterThreadHelper(Library library){
			this.library	= library;
			this.thread		= new Thread(this);
			this.thread.start();
		}
		
		public void run(){
			this.library.WriteThread(this);
		}
	}
	
	private WriterThreadHelper writerThreadHelper;
	  
	protected Library(){
	}
	
	public boolean Connect(String host,String username,String password,int queryPort,int httpPort){
		//Log.i("Library","starting  "+host+":"+queryPort);
		synchronized (notifier) {
			
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
	}
	
	public void WaitForAuthroization(){
		Log.v("Library::WaitForAuthroization","start");
		synchronized (notifier) {
			if(this.authorization.equals("")){
				try {
					notifier.wait();
				} catch (InterruptedException e) {
				}
			}
		}
		Log.v("Library::WaitForAuthroization","end");
	}
	
	public void run(){
		// First try to connect
		try{
			this.socket	= new java.net.Socket(this.host,this.queryPort);
			//Log.v("Library::socket","Successfully connected to "+this.host+":"+this.queryPort);
			
			doep.xml.Reader reader	= new doep.xml.Reader(this.socket.getInputStream());
			//Log.v("Library::run","Reader started");
			{
				// Wait for a "authentication" tag
				doep.xml.ReaderNode authNode=null;
				if( (authNode=reader.ChildNode("authentication")) != null ){
					//Log.v("Library::run","Authtag found");
					// Wait for authorization tag to end
					authNode.End();	
					
					synchronized (notifier) {
						Log.v("Library::run","Authtag end");
						this.authorization	= authNode.content;
						Log.v("Library::run","Authorization="+this.authorization);
						this.notifier.notifyAll();
						Log.v("Library::run","Authorization notify");
					}
				}
			}
			
			// Start write thread
			this.writerThreadHelper		= new WriterThreadHelper(this);
			
			// Lets start waiting for query-results
			//Log.v("Library::socket","Waiting for query results");
			
			doep.xml.ReaderNode queryNode	= null;
			while((queryNode=reader.ChildNode("queryresults"))!=null){
				//Log.v("NODE","We have a "+queryNode.name);
				// Find the right query
				IQuery query	= null;
				
				synchronized(this.waitingQueryQueue){
					java.util.ListIterator<IQuery> it=this.waitingQueryQueue.listIterator();
					String queryIdString	= queryNode.attributes.get("id");
					int queryId	= Integer.parseInt(queryIdString);
					while(it.hasNext() && query==null){
						query	= it.next();
						if(query.id!=queryId){
							query	= null;
						}else{
							// Remove it from the waitingQueue
							it.remove();
						}
					}
				}

				if(query!=null){
					//Log.v("Library::socket","Parse query results");
					// Parse the results
					query.ReceiveQueryResult(queryNode);
				}
				queryNode.End();
				
			}
			
			//Log.v("Library::socket","NOT Waiting for query results");
		}
		catch(IOException x){
			Log.e("Library::socket","IOE "+x.getMessage());
		}
		catch(Exception x){
			Log.e("Library::socket","E "+x.getMessage());
		}
		
	}
	
	public void WriteThread(WriterThreadHelper thread){
		//Log.v("Library::WriteThread","Started");
		try{
			doep.xml.Writer writer	= new doep.xml.Writer(this.socket.getOutputStream());
			{
				// Authenticate
				WriterNode authNode	= writer.ChildNode("authentication");
				authNode.attributes.put("username", this.username);
				authNode.content	= this.password;
				authNode.End();
			}
			
			// Wait for queries to send
			while(this.running){
				IQuery query	= null;
				try{
					synchronized(this.sendQueryQueue){
						if(this.sendQueryQueue.isEmpty()){
							this.sendQueryQueue.wait(2000);
							//Log.v("Library::WriteThread","wait over");
						}else{
							// Get the first query
							query	= this.sendQueryQueue.removeFirst();
						}
					}
				}
				catch(InterruptedException x){
					//Log.v("Library::WriteThread","Thread Notified");
				}
				
				if(query!=null){
					// Add query to waiting queries
					synchronized(this.waitingQueryQueue){
						this.waitingQueryQueue.addLast(query);
					}
					// Send the query
					query.SendQuery(writer);
				}
				
			}
			
			
		}
		catch(IOException x){
			Log.e("Library::WriteThread","IOException error");
		}
		catch(Exception x){
			Log.e("Library::WriteThread","E "+x.getMessage());
		}
		
		//Log.v("Library::WriteThread","Ended");
	}
	
	public void Disconnect(){
		this.running	= false;
		synchronized(this.sendQueryQueue){
			this.sendQueryQueue.notifyAll();
		}
		synchronized(this.waitingQueryQueue){
			this.waitingQueryQueue.notifyAll();
		}
//		this.writerThreadHelper.notifyAll();
		try{
			this.socket.close();
		}
		catch(Exception x){
			Log.e("Library::Disconnect","Exception error "+x.getMessage());
		}
		//this.writerThreadHelper.thread.join();
		//this.thread.join();
	}
	
	public void AddQuery(IQuery query){
		synchronized(this.sendQueryQueue){
			this.sendQueryQueue.addLast(query);
			this.sendQueryQueue.notifyAll();
		}
	}
	
}
