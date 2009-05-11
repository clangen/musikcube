/**
 * ...
 * @author Cube
 */
class core.query.ListSelection extends core.query.Base
{

	private var listMetaKeys:Array;
	private var selections:Array;
	private var listTracks:Boolean;
	
	public var ListEvent:core.Event;
	
	public function ListSelection() 
	{
		super();
		this.listMetaKeys	= new Array();
		this.selections		= new Array();
		this.ListEvent		= new core.Event();
		this.type			= "ListSelection";
		this.listTracks		= false;
	}

	public function ListTracks():Void {
		this.listTracks	= true;
	}
	
	public function ListMetaKey(key:String):Void {
		this.listMetaKeys.push(key);
	}
	
	public function AddSelection(key:String, selectIDs:Array):Void {
		
		for (var i:Number = 0; i < this.selections.length; i++) {
			if (this.selections[i].key == key) {
				this.selections[i].selection	= selectIDs;
				return;
			}
		}
		this.selections.push( { key:key,selection:selectIDs } );
	}
	
	public function GenerateQueryXML(xml) {
		trace("GenerateQueryXML "+this.selections.length+" "+this.listMetaKeys.length);
		var selections:XMLNode	= xml.createElement("selections");
		for (var i = 0; i < this.selections.length; i++) {
			var selection:XMLNode	= xml.createElement("selection");
			selection.attributes["key"]	= this.selections[i].key;
			var selectionValue:String	= "";
			for (var j = 0; j < this.selections[i].selection.length; j++) {
				selectionValue	+= (j != 0?",":"") + this.selections[i].selection[j];
			}
			selection.appendChild(xml.createTextNode(selectionValue));
			selections.appendChild(selection);
		}
		
		xml.firstChild.appendChild(selections);

		var listeners:XMLNode	= xml.createElement("listeners");
		var listenersContent:String	= "";
		for (var i = 0; i < this.listMetaKeys.length; i++) {
			listenersContent	+= (i != 0?",":"") + this.listMetaKeys[i];
		}
		listeners.appendChild(xml.createTextNode(listenersContent));
		
		xml.firstChild.appendChild(listeners);
		
		
		// List tracks
		if (this.listTracks) {
			var listTrackXML:XMLNode	= xml.createElement("listtracks");
			listTrackXML.appendChild(xml.createTextNode("true"));
			xml.firstChild.appendChild(listTrackXML);
		}
		
		return xml;
	}

	public function RecieveResult(xml:XMLNode):Void {
trace("RecieveResult");
		for (var i:Number = 0; i < xml.childNodes.length; i++) {
			if (xml.childNodes[i].nodeName == "metadata") {
				var key:String	= xml.childNodes[i].attributes["key"];
				var values:Array	= new Array();
				for (var j:Number = 0; j < xml.childNodes[i].childNodes.length; j++) {
					if (xml.childNodes[i].childNodes[j].nodeName == "md") {
						values.push({id:xml.childNodes[i].childNodes[j].attributes["id"],text:xml.childNodes[i].childNodes[j].firstChild.nodeValue});
					}
				}
trace("   RecieveResult "+key+" "+values.length);
				this.ListEvent.call2(key,values);
			}
		}
	}
	
}