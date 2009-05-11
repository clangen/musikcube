/**
 * ...
 * @author Cube
 */
class core.query.Base 
{

	public var id:Number		= 0;
	public var uid:Number		= 0;
	public var type:String		= "";
	public var options:Number	= 0;
	static var uidCounter:Number	= 0;

	public var ResultsRecievedEvent:core.Event;
	
	public function Base() 
	{
		core.query.Base.uidCounter++;
		
		this.id		= core.query.Base.uidCounter;
		this.uid	= core.query.Base.uidCounter;
		this.ResultsRecievedEvent	= new core.Event();
	}

	public function GenerateQueryXML(xml) {
		trace("ERROR, SHOULD NOT BE CALLED");
	}
	
	public function SendQuery():XML {
		var xml:XML	= new XML("<query id=\"" + this.id + "\" uid=\"" + this.uid + "\" type=\"" + this.type + "\" options=\"" + this.options + "\"></query>");
		return this.GenerateQueryXML(xml);
	}
	
	
}