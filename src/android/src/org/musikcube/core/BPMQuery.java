package org.musikcube.core;

import doep.xml.WriterNode;

public class BPMQuery extends ListQuery {

	public float queryForBPM	= 0;
	
	public BPMQuery() {
		super();
		this.type	= "BPMQuery";
		this.listTracks	= true;
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
		
		WriterNode bpmNode	= queryNode.ChildNode("bpm");
		bpmNode.content	= ""+this.queryForBPM;
		
		queryNode.End();
	}
	
}
