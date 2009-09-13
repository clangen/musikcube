package doep.xml;

import java.util.ListIterator;
import java.util.Map;

public class WriterNode {
	public final String name;
	public String content	= "";
	protected WriterNode parent;
	protected Writer writer;
	private final java.util.List<WriterNode> children	= new java.util.LinkedList<WriterNode>();
	public final java.util.SortedMap<String,String> attributes = new java.util.TreeMap<String,String>();
	
	protected int state	= 0;
	
	protected WriterNode(String name,WriterNode parent){
		this.name	= name;
		this.parent	= parent;
		if(parent!=null){
			this.writer	= parent.writer;
		}
	}
	
	public final WriterNode ChildNode(String name){
		final WriterNode newNode	= new WriterNode(name,this);
		this.children.add(newNode);
		return newNode;
	}
	
	public final void End()
		throws java.io.IOException
	{
		if(this.parent.state==0){
			// Parent node start-tag needs to be written first
			this.parent.WriteStartTag();
		}
		if(this.state==0){
			// Start tag has not been written yet
			this.WriteStartTag();
		}
		if(this.state==1){
			// Start tag has been written, lets "End" all children
			ListIterator<WriterNode> it	= this.children.listIterator();
			while(it.hasNext()){
				WriterNode child	= it.next();
				child.End();
			}
			// Remove the ended children
			this.children.clear();
			
			// Next: Write the content and the end-tag
			this.writer.Write(this.content+"</"+this.name+">");
			
			// If this is the root node, lets write a NULL and Flush
			if(this.parent==this.writer){
				this.writer.Flush(true);
			}
			
			// Finished
			this.state	= 2;
		}
	}
	
	protected final void WriteStartTag()
		throws java.io.IOException
	{
		if(this.state==0){
			String tag	= "<"+this.name;
			for(Map.Entry<String,String> entry : this.attributes.entrySet()){
				tag	+= " "+entry.getKey()+"=\""+entry.getValue()+"\"";
			}
			// Clear the attributes
			this.attributes.clear();
			
			tag	+= ">";
	
			// Start tag written
			this.state	= 1;
			
			this.writer.Write(tag);
		}
	}
	
	
}
