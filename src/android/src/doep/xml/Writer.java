package doep.xml;

import android.util.Log;


public class Writer extends WriterNode {

	public String buffer;
	private java.io.OutputStream stream;
	
	public Writer(java.io.OutputStream stream){
		super("",null);
		this.parent	= this;
		this.writer	= this;
		this.stream	= stream;
		this.state	= 1;
	}
	
	public void Write(String content)
		throws java.io.IOException
	{
		Log.v("doep.xml.Writer","Write "+content);
		this.stream.write(content.getBytes());
//		this.buffer		+= content;
	}
	
	public void Flush()
		throws java.io.IOException
	{
		this.Flush(false);
	}
	
	public void Flush(boolean writeNull)
		throws java.io.IOException
	{
		Log.v("doep.xml.Writer","Flush "+writeNull);
		if(writeNull){
			this.stream.write(0);
		}
		this.buffer	= "";
		this.stream.flush();
	}
	
}
