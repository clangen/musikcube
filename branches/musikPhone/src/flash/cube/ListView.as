/**
 * ...
 * @author ...
 */
class cube.ListView extends MovieClip
{
	
	public var items:Array;
	
	private var startIndex:Number	= 0;
	private var endIndex:Number		= 0;
	private var activeIndex:Number	= 0;
	private var itemHeight:Number	= 18;
	
	private var height:Number;
	private var width:Number;
	
	private var currentY:Number		= 0;
	private var gotoY:Number		= 0;
	
	public var EnterEvent:core.Event;
	public var BackEvent:core.Event;
	public var ActivateEvent:core.Event;
	
	public function ListView() 
	{
//		this.mcItems	= new Array();
		this.items		= new Array();
		this.EnterEvent	= new core.Event();
		this.BackEvent	= new core.Event();
		this.ActivateEvent	= new core.Event();
	}
	
	public function AddItem(text:String, id:String):Void {
//		var newItem	= this.attachMovie("ListViewItem", "item" + this.items.length, this.getNextHighestDepth());
		this.items.push({id:id,text:text});
//		newItem.Initialize(id, text);
	}
	
	private function ResetItems():Void {
		for (var i = this.startIndex; i < this.endIndex; i++) {
			this["item"+i].removeMovieClip()
		}
	}
	
	public function Initialize(width:Number, height:Number):Void {
		this.width	= width;
		this.height	= height;
		
		this.ResetItems();
		this.SetEnterFrame();				
	}
	
	public function ScrollToLine(lineNumber):Void {
		//trace("ScrollToLine "+this.height+" "+this.itemHeight);
		this.gotoY	= lineNumber * this.itemHeight -(this.height - this.itemHeight) / 2;
		
		if (this.gotoY > this.itemHeight * this.items.length - this.height ) {
			this.gotoY = this.itemHeight * this.items.length - this.height;
		}
		if (this.gotoY < 0) {
			this.gotoY = 0;
		}
		
		this.SetEnterFrame();
	}
	
	public function SetEnterFrame():Void {
		this.onEnterFrame	= function () {
			var newY:Number	= Math.round(this.currentY + (this.gotoY - this.currentY) * 0.51);
			if (newY == this.currentY) {
				this.onEnterFrame	= undefined;
			}
			this.currentY	= newY;
			this.Render();			
		}
	}
	
	public function Render():Void {
		var newStartIndex:Number	= Math.floor(this.currentY / this.itemHeight);
		var newEndIndex:Number		= newStartIndex + Math.ceil(this.height / this.itemHeight) + 1;
		if (newEndIndex > this.items.length) {
			newEndIndex = this.items.length;
		}
		
		// start by removing unwanted items
		for (var i:Number = this.startIndex; i < this.endIndex; i++) {
			if (i < newStartIndex || i > newEndIndex) {
				this["item" + i].removeMovieClip();
			}
		}
		
		this.startIndex	= newStartIndex;
		this.endIndex	= newEndIndex;
		
		// then create the once that are not created
		for (var i:Number = this.startIndex; i < this.endIndex; i++) {
			if (!this["item" + i]) {
				var newItem	= this.attachMovie("ListViewItem", "item" + i, this.getNextHighestDepth());
				newItem.Initialize(this.items[i]);
				if (this.activeIndex == i) {
					newItem.Activate(i);
				}
			}
			
			// and place them
			this["item" + i].SetPosition(0,i*this.itemHeight-this.currentY,this.width,this.itemHeight,i%2==0,i==this.activeIndex);

		}

	}
	
	public function Activate():Void {
		Key.addListener(this);
		this.ActivateEvent.call1(true);
		this.ActivateItem(this.activeIndex);
	}
	
	public function Deactivate():Void {
		Key.removeListener(this);
	}
	
	private function DeactivateItem(itemIndex:Number):Void {
		//trace("DeactivateItem "+itemIndex+" "+this["item" + itemIndex]);
		if (this["item" + itemIndex]) {
			this["item" + itemIndex].Deactivate(itemIndex);
		}
	}
	private function ActivateItem(itemIndex:Number):Void {
		//trace("ActivateItem "+itemIndex+" "+this["item" + itemIndex]);
		if (this["item" + itemIndex]) {
			this["item" + itemIndex].Activate(itemIndex);
		}
		this.ScrollToLine(this.activeIndex);
	}
	
	public function onKeyDown():Void {
		//trace("keydown " + Key.getCode());
		this.DeactivateItem(this.activeIndex);
		
		switch(Key.getCode()) {
			case Key.DOWN:
				this.activeIndex++;
				if (this.activeIndex >= this.items.length) {
					this.activeIndex	= this.items.length-1;
				}
				break;
			case Key.UP:
				this.activeIndex--;
				if (this.activeIndex<0) {
					this.activeIndex	= 0;
				}
				break;
			case Key.ENTER:
			case Key.RIGHT:
				this.Deactivate();
				this.EnterEvent.call0();
				return;
				break;
			case Key.LEFT:
				this.Deactivate();
				this.BackEvent.call0();
				return;
				break;
		}
		this.ActivateItem(this.activeIndex);
		
	}
	
	public function GetSelectedId() {
		return this.items[this.activeIndex].id;
	}
		
}