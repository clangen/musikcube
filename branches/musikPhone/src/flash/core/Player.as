/**
 * ...
 * @author Daniel Önnerby
 */
class core.Player extends MovieClip
{

	static var instance:core.Player;	// Singleton instance
	private var tracklist:Array;
	private var library:core.Library;
	
	private var tracklistIndex:Number	= -1;
	
	public var sound:Sound;
	
	public function Player() 
	{
		core.Player.instance	= this;
		this.tracklist			= new Array();
		this.sound				= new Sound(this);
//		this.sound.onLoad		= function (success) { core.Player.instance.sound.start(); }
	}
	
	public function Initialize(library:core.Library) :Void {
		this.library	= library;
	}
	
	public function PlayTracks(tracklist:Array,startIndex:Number) {
		this.tracklist	= tracklist;
		this.tracklistIndex	= startIndex-1;
		this.Play();
	}
	
	public function Play():Void {
		this.tracklistIndex++;
		var url = "http://" + this.library.host + ":" + this.library.httpPort + "/track/" + this.tracklist[this.tracklistIndex] + ".mp3?track_id=" + this.tracklist[this.tracklistIndex] + "&auth_key=" + this.library.authentication;
_level0["cube"]["thelog"].text += "Player::PlayTracks: " + url+"\n";
		this.sound.loadSound(url, true);
		this.sound.start();
	}
	
	
}