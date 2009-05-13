/**
 * ...
 * @author ...
 */
class cube.MetakeyLister 
{

	public var library:core.Library;
	public var metakeyOrder:Array;
	private var listViews:Array;
	private var parent;
	private var currentView:Number		= -1;
	
	public function MetakeyLister(library:core.Library,metakeyOrder:Array,parent) 
	{
		this.library		= library;
		this.metakeyOrder	= metakeyOrder;
		this.parent 		= parent;
		
		this.listViews		= new Array();
		
	}
	
	public function Start():Void {
		this.NextView();
	}
	
	public function Resize():Void {
		this.listViews[this.currentView].Initialize(Stage.width, Stage.height);
	}
	
	private function NextView():Void {
		this.currentView++;
		if (this.currentView > this.metakeyOrder.length) {
			this.currentView--;
			this.listViews[this.currentView].Activate();
			
			// Get all tracks
			var tracklist:Array	= new Array();
			for (var i = 0; i < this.listViews[this.currentView].items.length; i++) {
				tracklist.push(this.listViews[this.currentView].items[i].id);
			}
_level0["cube"]["thelog"].text += "PLAY: " + tracklist.length+"\n";
			// Play the tracks
			core.Player.instance.PlayTracks(tracklist, this.listViews[this.currentView].activeIndex);
			
			return;
		}
		
		var listview:Object	= undefined;
		if (this.currentView == this.metakeyOrder.length) {
			// Tracklist
			listview	= this.parent.attachMovie("TrackList", "listview"+this.currentView, this.parent.getNextHighestDepth());
		}else {
			// Metakeylist
			listview	= this.parent.attachMovie("MetakeyList", "listview"+this.currentView, this.parent.getNextHighestDepth());
			listview.SetMetakey(this.metakeyOrder[this.currentView]);
		}
		listview.Initialize(Stage.width, Stage.height);
		
		// Loop through selections
		for (var i:Number = 0; i < this.currentView; i++) {
			listview.query.AddSelection(metakeyOrder[i], [this.listViews[i].GetSelectedId()]);
//			this.listViews[i]._visible	= false;
//			this.listViews[i].ResetItems();
		}
		listview.EnterEvent.addListener(this, this.NextView);
		listview.BackEvent.addListener(this, this.BackView);
		listview.ActivateEvent.addListener(this, this.CurrentViewActivated);
		
		this.listViews[this.currentView]	= listview;
		
		// Activate the loader
		var loader:MovieClip	= this.parent._parent.attachMovie("loading", "loading", this.parent._parent.getNextHighestDepth());
		loader._x	= Stage.width / 2;
		loader._y	= Stage.height / 2;
		
		listview.SendQuery(this.library);
	}
	
	public function CurrentViewActivated(active):Void {
		for (var i:Number = 0; i < this.currentView; i++) {
			this.listViews[i]._visible	= false;
			this.listViews[i].ResetItems();
		}
		this.parent._parent["loading"].removeMovieClip();
	}

	public function BackView():Void {
		this.currentView--;
		if (this.currentView<0) {
			this.currentView++;
			this.listViews[this.currentView].Activate();
			return;
		}
		
		// remove the old view
		this.listViews[this.currentView + 1].removeMovieClip();
		
		this.listViews[this.currentView].Initialize(Stage.width, Stage.height);
		this.listViews[this.currentView].Activate();
		this.listViews[this.currentView]._visible	= true;
		this.listViews[this.currentView].SetEnterFrame();
	}
	
}