package org.musikcube.core;

import doep.xml.ReaderNode;
import doep.xml.WriterNode;

public class MetadataQuery extends IQuery {
	
	public java.util.ArrayList<Integer> requestedTracks	= new java.util.ArrayList<Integer>(); 
	public java.util.ArrayList<String> requestedMetakeys		= new java.util.ArrayList<String>(); 
	public java.util.ArrayList<Track> resultTracks			= new java.util.ArrayList<Track>(); 
	
	public MetadataQuery() {
		super();
		this.type	= "TrackMetadata";
	}


	@Override 	
	public void SendQuery(WriterNode node)
		throws Exception 
	{
		WriterNode queryNode	= this.SendQueryNode(node);
		
		WriterNode metakeysNode	= queryNode.ChildNode("metakeys");
		
		String metakeys		= "";
		int metakeysCount	= this.requestedMetakeys.size();
		for(int i=0;i<metakeysCount;i++){
			if(i!=0){
				metakeys	+= ",";
			}
			metakeys	+= this.requestedMetakeys.get(i);
		}
		metakeysNode.content	= metakeys;

		WriterNode tracksNode	= queryNode.ChildNode("tracks");
		String tracks	= "";
		int tracksCount	= this.requestedTracks.size();
		for(int i=0;i<tracksCount;i++){
			if(i!=0){
				tracks	+= ",";
			}
			tracks	+= this.requestedTracks.get(i).toString();
		}
		tracksNode.content	= tracks;
		
		queryNode.End();
	}
	
	@Override 	
	public void ReceiveQueryResult(ReaderNode node)
		throws Exception
	{
		
		ReaderNode trackNode	= null;
		while( (trackNode=node.ChildNode("t"))!=null ){
			Track track	= new Track();
			track.id	= Integer.parseInt(trackNode.attributes.get("id"));
			
			ReaderNode mdNode	= null;
			while( (mdNode=trackNode.ChildNode("md"))!=null ){
				mdNode.End();
				track.metadata.put(mdNode.attributes.get("k"), mdNode.content);
			}
		}
		
		if(this.listener!=null){
			this.listener.OnQueryResults();
		}
	}

}
