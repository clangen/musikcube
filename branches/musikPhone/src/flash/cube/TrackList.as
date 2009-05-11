/**
 * ...
 * @author ...
 */
class cube.TrackList extends cube.ListView
{
	public var query:core.query.ListSelection;
	
	public function TrackList() 
	{
		super();
		this.query	= new core.query.ListSelection();
		this.query.ListEvent.addListener(this, this.RecieveResults);
		this.query.ListTracks();
	}
	
	public function SendQuery(library:core.Library):Void {
		this.ResetItems();
		this.items	= new Array();
		
		library.SendQuery(this.query);
	}
	
}