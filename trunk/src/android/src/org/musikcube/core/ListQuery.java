package org.musikcube.core;

import doep.xml.ReaderNode;
import doep.xml.WriterNode;

public class ListQuery extends IQuery {
	
	public String category		= "";
	protected java.util.ArrayList<String> selectionStrings	= new java.util.ArrayList<String>(); 
	protected java.util.ArrayList<Integer> selectionInts 		= new java.util.ArrayList<Integer>(); 
	public java.util.ArrayList<String> resultsStrings		= new java.util.ArrayList<String>(); 
	public java.util.ArrayList<Integer> resultsInts			= new java.util.ArrayList<Integer>(); 
	public java.util.ArrayList<Integer> trackList			= new java.util.ArrayList<Integer>(); 
	public boolean listTracks	= false;	
	

	public ListQuery() {
		super();
		this.type	= "ListSelection";
	}
	
	@Override 	
	public void SendQuery(WriterNode node)
		throws Exception 
	{
		WriterNode queryNode	= this.SendQueryNode(node);
		
		// List selections
		WriterNode selectionsNode	= queryNode.ChildNode("selections");
		int selectionCount	= this.selectionStrings.size();
		for(int i=0;i<selectionCount;i++){
			WriterNode selectionNode	= selectionsNode.ChildNode("selection");
			selectionNode.attributes.put("key", this.selectionStrings.get(i));
			selectionNode.content	= this.selectionInts.get(i).toString();
		}
		
		// What category to listen for
		WriterNode listenersNode	= queryNode.ChildNode("listeners");
		listenersNode.content		= this.category;
		
		// List tracks?
		if(this.listTracks){
			WriterNode listtracksNode	= queryNode.ChildNode("listtracks");
			listtracksNode.content	= "true";
		}
		
		queryNode.End();
	}
	
	@Override 	
	public void ReceiveQueryResult(ReaderNode node)
		throws Exception
	{
		// Get metadata-tag
		ReaderNode childNode	= null;
		while( (childNode=node.ChildNode())!=null ){
			if(childNode.name.equals("metadata")){
				if(childNode.attributes.get("key").equals(this.category)){
					// This is the list we are looking for (same category)
					ReaderNode mdNode	= null;
					while( (mdNode=childNode.ChildNode("md"))!=null ){
						mdNode.End();
						this.resultsInts.add(Integer.parseInt(mdNode.attributes.get("id")));
						this.resultsStrings.add(mdNode.content);
					}
				}
			}else if(childNode.name.equals("tracklist")){
				// Get tracks
				ReaderNode trackNode	= null;
				while( (trackNode=childNode.ChildNode("tracks"))!=null ){
					trackNode.End();	
					String[] tracks	= trackNode.content.split(",");
					int nofTracks	= tracks.length;
					for(int i=0;i<nofTracks;i++){
						this.trackList.add(Integer.parseInt(tracks[i]));
					}
				}
			}
			childNode.End();
		}
		
		// TODO: Notify that results are here
		if(this.listener!=null){
			this.listener.OnQueryResults(this);
		}
	}

	public void SelectData(String category,int selection){
		this.selectionStrings.add(category);
		this.selectionInts.add(selection);
	}

}
