/**
 * ...
 * @author ...
 */
class cube.ListViewItem extends MovieClip
{
	
	public var id;
	
	public function ListViewItem() 
	{
		
	}
	
	public function Initialize(initObject:Object):Void {
		this.id = initObject.id;
		this["thetext"].text	= initObject.text;
	}
	
	public function SetPosition(x:Number, y:Number, width:Number, height:Number,evenRow:Boolean,active:Boolean):Void {
		this._x	= x;
		this._y	= y;
		
		this["bg"].gotoAndStop(active?3:(evenRow?1:2));
		this["bg"]._width	= width;
		this["bg"]._height	= height;
	}
	
	public function Deactivate(row):Void {
		this["bg"].gotoAndStop(row%2==0?1:2);
	}
	public function Activate(row):Void {
		this["bg"].gotoAndStop(3);		
	}
	
}