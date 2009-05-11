/**
 * ...
 * @author ...
 */
class cube.MetakeyList extends cube.ListView
{
	private var metaKey:String		= "";
	public var query:core.query.ListSelection;
	
	public function MetakeyList() 
	{
		super();
		this.query	= new core.query.ListSelection();
		this.query.ListEvent.addListener(this, this.RecieveResults);
		this.query.ResultsRecievedEvent.addListener(this, this.ResultsRecieved);
	}
	
	public function SetMetakey(key:String):Void {
		this.metaKey	= key;
		this.query.ListMetaKey(key);
	}
	
	public function SendQuery(library:core.Library):Void {
		this.ResetItems();
		this.items	= new Array();
		
		library.SendQuery(this.query);
	}
	
	private function RecieveResults(key, values):Void {
		//trace("RecieveResults " + key+ " "+this);
		if (key == this.metaKey) {
			if(this.items.length==0){
				this.items	= values;
			}else {
				for (var i = 0; i < values.length; i++) {
					this.items.push(values[i]);
				}
			}
		}
	}
	
	private function ResultsRecieved():Void {
		this.ScrollToLine(0);
		this.Activate();
	}
}