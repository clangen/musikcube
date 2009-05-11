/**
 * ...
 * @author Daniel Önnerby
 */
class core.query.TrackMetadata extends core.query.Base
{
	
	private var requestMetakeys:Array;
	private var requestTracks:Array;
	public var TrackMetaEvent:core.Event;
	
	public function TrackMetadata() 
	{
		super();
		this.requestMetakeys	= new Array();
		this.TrackMetaEvent		= new core.Event();
	}
	
	public function RequestMetakeys(metakeys:Array):Void {
		this.requestMetakeys	= metakeys;
	}
	
	public function RequestTracks(tracks:Array):Void {
		//trace("TrackMetadata::RequestTracks "+tracks.length);
		this.requestTracks	= tracks;
	}

	public function GenerateQueryXML(xml) {
		//trace("GenerateQueryXML "+this.requestMetakeys.length+" "+this.requestTracks.length);
		var metakeysXML:XMLNode	= xml.createElement("metakeys");
		var metakeys:String	= "";
		for (var i = 0; i < this.requestMetakeys.length; i++) {
			metakeys	+= (i != 0?",":"") + this.requestMetakeys[i];
		}
		metakeysXML.appendChild(xml.createTextNode(metakeys));
		xml.firstChild.appendChild(metakeysXML);


		var tracksXML:XMLNode	= xml.createElement("tracks");
		var tracks:String	= "";
		for (var i = 0; i < this.requestTracks.length; i++) {
			tracks	+= (i != 0?",":"") + this.requestTracks[i];
		}
		tracksXML.appendChild(xml.createTextNode(tracks));
		xml.firstChild.appendChild(tracksXML);
		return xml;
	}
	
	public function RecieveResult(xml:XMLNode):Void {
		
		this.ResultsRecievedEvent.call0();
	}
	
}