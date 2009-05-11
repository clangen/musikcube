/**
 * ...
 * @author ...
 */
class core.Event 
{
	
	public var listeners:Array;
	
	public function Event() 
	{
		this.listeners	= new Array();
	}
	
	public function addListener(listenerObject:Object, method:Function):Void {
		this.listeners[this.listeners.length]	= new Array(listenerObject, method);
	}
	
	public function clear():Void {
		this.listeners	= new Array();
	}
	
	public function call0():Void {
		for (var i = 0; i < this.listeners.length; i++) {
			this.listeners[i][1].call(this.listeners[i][0]);
		}
	}
	public function call1(param1):Void {
		for (var i = 0; i < this.listeners.length; i++) {
			this.listeners[i][1].call(this.listeners[i][0],param1);
		}
	}
	public function call2(param1, param2):Void {
		trace(" event "+this.listeners.length);
		for (var i = 0; i < this.listeners.length; i++) {
			this.listeners[i][1].call(this.listeners[i][0],param1,param2);
		}
	}
	
}