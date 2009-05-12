/**
 * ...
 * @author ...
 */
class core.Library 
{
	public var username:String		= "";
	public var password:String		= "";
	public var authentication:String= "";
	public var host:String			= "";
//	public var host:String			= "localhost";
	public var queryPort:Number		= 10543;
	public var httpPort:Number		= 10544;
	
	private var socket:XMLSocket;
//	private var viewOrder:Array;

	private var lastError:String	= "";
	private var sessionId:String	= "";
	
	public var statusChangeEvent:core.Event;
	public var connected:Boolean	= false;
	
	private var queries:Array;
	
	public function Library() 
	{
		trace("Library");
		this.statusChangeEvent	= new core.Event();
		this.socket				= new XMLSocket();
		this.queries			= new Array();
//		this.viewOrder	= new Array("genre", "artist", "album");
	}
	
	public function Connect():Boolean {
		
		// first read policy
		this.statusChangeEvent.call2("security", this);
		System.security.loadPolicyFile("http://" + this.host + ":" + this.httpPort + "/crossdomain.xml");
		
		
		trace("Library::Connect");
		this.socket["library"]	= this;
		this.socket.onClose		= function() { this.library.SocketClosed(); }
		this.socket.onConnect	= function(success:Boolean) { this.library.SocketConnect(success); }
		this.socket.onXML		= function(xml:XML) { this.library.InitSocketRecieve(xml); }
		this.socket.onData		= function(data) { 
			_level0["cube"]["thelog"].text = "R: " + data;
			this.onXML(new XML(data));
		}
		
		if (!this.socket.connect(this.host, this.queryPort)) {
			this.SetError("Unable to connect to " + this.host + ":" + this.queryPort);
			this.statusChangeEvent.call2("error",this);
			return false;
		}
		
		return true;
	}
	
	public function SocketClosed():Void {
		trace("Library::SocketClosed");
		_level0["cube"]["thelog"].text 	+= "Library::SocketClosed";
		
		this.statusChangeEvent.call2("closed",this);
	}

	public function SocketConnect(success:Boolean):Void {
		trace("Library::SocketConnect "+success);
		if(success){
			this.statusChangeEvent.call2("connected", this);
			// Waiting for authorization id
			this.connected	= true;
		}else {
			this.SetError("Unable to connect to " + this.host + ":" + this.queryPort);
			this.statusChangeEvent.call2("error", this);
		}
	}
	
	private function CryptPassword():String{
		return this.password;
	}
	
	public function InitSocketRecieve(xml:XML):Void {
		trace("Library::InitSocketRecieve " + xml.firstChild.toString());
		if (xml.firstChild.nodeName == "authentication") {
			this.authentication	= xml.firstChild.nodeValue;
		}
		this.socket.onXML		= function(xml:XML) { this.library.SocketRecieve(xml); }
		
		// Lets authorize
		var authXML:XML		= new XML("<authentication username=\"" + this.username + "\">"+this.CryptPassword()+"</authentication>");
		this.socket.send(authXML);
		trace(" send "+authXML);
		this.statusChangeEvent.call2("authorized",this);
		
	}
	public function SocketRecieve(xml:XML):Void {
//		trace("Library::SocketRecieve " + xml.firstChild.toString());
//		trace("Library::SocketRecieve " );
_level0["cube"]["thelog"].text 	= xml.toString();
		
		var queryId:Number	= Number(xml.firstChild.attributes["id"]);
//		var queryUId:Number	= Number(xml.firstChild.attributes["uid"]);
		
		for (var i:Number = 0; i < this.queries.length; i++) {
			if (this.queries[i].id == queryId) {
				
				this.queries[i].RecieveResult(xml.firstChild);
				
				//remove query
				this.queries.splice(i, 1);
				i--;
			}
		}
		
	}
	
	private function SetError(error:String):Void {
		trace("Library::SetError " + error);
		this.lastError	= error;
	}
	
	public function GetLastError():String {
		return this.lastError;
	}
	
	public function SendQuery(query):Void {
		this.queries.push(query);
		
		var queryXML:XML	= query.SendQuery();
		//trace("Library::SendQuery " + queryXML.toString());
//_level0["cube"]["thelog"].text 	= queryXML.toString();
		this.socket.send(queryXML);
	}
	
}