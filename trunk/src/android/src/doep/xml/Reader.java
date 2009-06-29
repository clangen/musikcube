package doep.xml;
import org.xmlpull.v1.*;

import android.util.Log;
import doep.xml.ReaderNode;
import java.io.InputStream;
import java.util.LinkedList;
import java.lang.Exception;


public class Reader extends ReaderNode {
	private InputStream stream;
	private org.xmlpull.v1.XmlPullParser parser;
	private java.util.LinkedList<ReaderNode> nodeLevels	= new java.util.LinkedList<ReaderNode>();
	public ReaderNode currentNode;
	private boolean firstParse	= true;

	
	public Reader(InputStream stream)
		throws Exception
	{
		super("",null);
		this.stream		= stream;
		this.reader		= this;
		
		XmlPullParserFactory factory = XmlPullParserFactory.newInstance();
        factory.setNamespaceAware(false);
        this.parser	= factory.newPullParser();
		this.parser.setInput(this.stream, null);
		
		// Add the Reader to the nodeLevels itself
		this.nodeLevels.addLast(this);
		this.currentNode	= this;
	}
	

	public void Parse()
		throws Exception
	{
		int eventType	= 0;
		if(this.firstParse){
			this.firstParse	= false;
			eventType = this.parser.getEventType();
		}else{
			eventType = this.parser.next();
		}
			
//        while (eventType != XmlPullParser.END_DOCUMENT) {
			if(eventType == XmlPullParser.START_DOCUMENT) {
				//Log.v("doep::Reader::Parse","Start document");
				// Start document
			} else if(eventType == XmlPullParser.END_DOCUMENT) {
				// End document
				//Log.v("doep::Reader::Parse","End document");
				
				java.util.ListIterator<ReaderNode> it=this.nodeLevels.listIterator();
				while(it.hasNext()){
					it.next().ended	= true;
				}
				
			} else if(eventType == XmlPullParser.START_TAG) {
				//Log.v("doep::Reader::Parse","Start tag "+this.parser.getName());
			    //System.out.println("Start tag "+xpp.getName());
				// Start a new node
				ReaderNode node	= new ReaderNode(this.parser.getName(),this.nodeLevels.getLast());
				
				// Get the attributes
				int attributes	= this.parser.getAttributeCount();
				for(int i=0;i<attributes;i++){
					node.attributes.put(this.parser.getAttributeName(i),this.parser.getAttributeValue(i));
				}
				
				// Add to the end of the levels
				this.nodeLevels.addLast(node);
				node.level	= this.nodeLevels.size();
				// Set to current node for futher processing
				this.currentNode	= node;
				
			} else if(eventType == XmlPullParser.END_TAG) {
				//Log.v("doep::Reader::Parse","End tag "+this.parser.getName());
			    //System.out.println("End tag "+xpp.getName());
				if(this.parser.getName().equals(this.currentNode.name)){
					// End the node, and remove from levels
					this.currentNode.ended	= true;
					this.nodeLevels.removeLast();
					this.currentNode	= this.nodeLevels.getLast();
				}else{
					// Something is wrong.. wrong end tag
					throw new Exception("Wrong end tag.. expecting "+this.currentNode.name);
				}
			} else if(eventType == XmlPullParser.TEXT) {
				//Log.v("doep::Reader::Parse","Text "+this.parser.getText());
			    //System.out.println("Text "+xpp.getText());
				this.currentNode.content	+= this.parser.getText();
			}
//        }
	}
	

}

