package doep.xml;
import doep.xml.Reader;

public class ReaderNode {

	public final String name;
	public boolean ended	= false;
	public String content	= "";
	public final ReaderNode parent;
	public Reader reader;
	public int level		= 1;
	
	public java.util.SortedMap<String,String> attributes	= new java.util.TreeMap<String,String>();
	
	public ReaderNode(String name,ReaderNode parent){
		this.name	= name;
		this.parent	= parent;
		if(parent!=null){
			this.reader	= parent.reader;
		}
	}

	/**
	 * Get attribute from node
	 * @param attribute
	 * @return String or null
	 */
	public final String Attribute(String attribute){
		return this.attributes.get(attribute);
	}
	
	/**
	 * Wait for a childnode with the specific name
	 * @param name Name of node to be found
	 * @return ReaderNode if found, or null if this goes out of scope
	 */
	public final ReaderNode ChildNode(String name)
		throws Exception
	{
		//Log.v("doep::ReaderNode::ChildNode","Node: "+name+" ended: "+this.ended);
		// Check if this is the current node, if not - lets wait for it to bee
		while(!this.ended){
			this.reader.Parse();
			//Log.v("doep::ReaderNode::ChildNode1","Node: "+this.reader.currentNode.level+" "+this.level);
			if(this.reader.currentNode.level==this.level+1){
				// We have a childnode
				//Log.v("doep::ReaderNode::ChildNode2",""+this.reader.currentNode.name+"=="+name);
				if(this.reader.currentNode.name.equals(name)){
					//Log.v("doep::ReaderNode::ChildNode3","FOUND "+this.reader.currentNode.name);
					return this.reader.currentNode;
				}else{
					this.reader.currentNode.End();
				}
			}else if( this.reader.currentNode.level>this.level+1){
				this.reader.currentNode.End();
			}
		}
		//Log.v("doep::ReaderNode::ChildNode4","NOT FOUND "+name);
		return null;
	}
	
	public final void End()
		throws Exception
	{
		while(!this.ended){
			//Log.v("doep::ReaderNode::End","Node: "+this.name+" "+this.level);
			this.reader.Parse();
		}
		//Log.v("doep::ReaderNode::End","Ended");
	}
	
	/**
	 * Wait for a childnode
	 * @return ReaderNode if found, or null if this goes out of scope
	 */
	public final ReaderNode ChildNode()
		throws Exception
	{
		while(!this.ended){
			this.reader.Parse();
			if(this.reader.currentNode.level==this.level+1){
				// We have a childnode
				return this.reader.currentNode;
			}else if( this.reader.currentNode.level>this.level+1){
				this.reader.currentNode.End();
			}
		}
		return null;
	}
	
}
