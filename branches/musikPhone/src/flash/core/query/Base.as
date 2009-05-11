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
	
	public function Base() 
	{
		core.query.Base.uidCounter++;
		
		this.id		= core.query.Base.uidCounter;
		this.uid	= core.query.Base.uidCounter;
	}

	public function GenerateQueryXML(xml) {
		trace("ERROR, SHOULD NOT BE CALLED");
	}
	
	public function SendQuery():XML {
		var xml:XML	= new XML("<query id=\"" + this.id + "\" uid=\"" + this.uid + "\" type=\"" + this.type + "\" options=\"" + this.options + "\"></query>");
		trace("query.Base.SendQuery "+xml.toString());
		xml	= this.GenerateQueryXML(xml);
		trace("SendQuery "+xml.toString());
		return xml;
	}
	
	
}