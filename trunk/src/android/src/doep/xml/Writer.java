package doep.xml;


public final class Writer extends WriterNode {

	public String buffer;
	private final java.io.OutputStream stream;
	
	public Writer(java.io.OutputStream stream){
		super("",null);
		this.parent	= this;
		this.writer	= this;
		this.stream	= stream;
		this.state	= 1;
	}
	
	public final void Write(String content)
		throws java.io.IOException
	{
		//Log.v("doep.xml.Writer","Write "+content);
		this.stream.write(content.getBytes());
//		this.buffer		+= content;
	}
	
	public final void Flush()
		throws java.io.IOException
	{
		this.Flush(false);
	}
	
	public final void Flush(boolean writeNull)
		throws java.io.IOException
	{
		//Log.v("doep.xml.Writer","Flush "+writeNull);
		if(writeNull){
			this.stream.write(0);
		}
		this.buffer	= "";
		this.stream.flush();
	}
	
}
