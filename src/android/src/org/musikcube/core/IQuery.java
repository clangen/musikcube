package org.musikcube.core;

import java.io.IOException;

import doep.xml.*;

public class IQuery extends Object{

	public int id		= 0;
	public int uid		= 0;
	public String type	= "";
	public int options	= 0;
	public static int uidCounter	= 0; 
	
	public interface OnQueryResultListener{
		public void OnQueryResults();
	}
	
	protected OnQueryResultListener listener	= null;
	
	public IQuery(){
		IQuery.uidCounter++;
		this.id	= IQuery.uidCounter;
		this.uid	= IQuery.uidCounter;
	}

	protected WriterNode SendQueryNode(WriterNode parentNode)
	{
		WriterNode queryNode	= parentNode.ChildNode("query");
		queryNode.attributes.put("id",Integer.toString(this.id));
		queryNode.attributes.put("uid",Integer.toString(this.uid));
		queryNode.attributes.put("type",this.type);
		queryNode.attributes.put("options",Integer.toString(this.options));
		return queryNode;
	}
	
	public void SendQuery(WriterNode node)
		throws Exception 
	{
	}
	
	public void ReceiveQueryResult(ReaderNode node)
		throws Exception
	{
		
	}
	
	public void SetResultListener(OnQueryResultListener listener){
		this.listener	= listener;
	}
	
}
