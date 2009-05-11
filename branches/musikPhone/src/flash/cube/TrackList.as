/**
 * ...
 * @author ...
 */
class cube.TrackList extends cube.ListView
{
	public var query:core.query.ListSelection;
	private var metadataQuery:core.query.TrackMetadata;
	private var library:core.Library;
	
	public function TrackList() 
	{
		super();
		this.query	= new core.query.ListSelection();
		this.query.TracksEvent.addListener(this, this.RecieveResults);
		this.query.ResultsRecievedEvent.addListener(this, this.TrackListRecieved);
		this.query.ListTracks();
		
		this.metadataQuery	= new core.query.TrackMetadata();
		this.metadataQuery.RequestMetakeys(["track", "title"]);
		this.metadataQuery.ResultsRecievedEvent.addListener(this, this.TrackMetadataRecieved);
	}
	
	public function SendQuery(library:core.Library):Void {
		//trace("Tracklist::SendQuery");
		this.library	= library;
		this.ResetItems();
		this.items	= new Array();
		
		library.SendQuery(this.query);
	}
	
	public function RecieveResults(tracks:Array):Void {
		//trace("Tracklist::RecieveResults "+tracks.length);
		for (var i:Number = 0; i < tracks.length; i++) {
			this.items.push({id:tracks[i],text:"track "+tracks[i]});
		}
	}
	
	public function TrackListRecieved():Void {
		trace("Tracklist::TrackListRecieved "+this.library);

		
		//lets query for the metadata we want
		var tracks:Array	= new Array();
		for (var i:Number = 0; i < this.items.length; i++) {
			tracks.push(this.items[i].id);
		}
				
		this.metadataQuery.RequestTracks(tracks);
		this.library.SendQuery(this.metadataQuery);
				
	}
	
	public function TrackMetadataRecieved():Void {
		this.ScrollToLine(0);
		this.Activate();
	}
	
}