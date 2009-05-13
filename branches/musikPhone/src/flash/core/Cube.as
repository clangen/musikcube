/**
 * ...
 * @author Cube
 */
class core.Cube extends MovieClip
{
	private var library:core.Library;
	private var testcount:Number = 0;
	private var retries:Number		= 4;
	
	private var currentView;
	private var saveData:SharedObject;
	
	public function Cube() 
	{
		Stage.scaleMode	= "noScale";
		Stage.align		= "TL";
		this.library	= new core.Library();
		fscommand2("FullScreen", true);
	}
	
	public function onLoad():Void {
		trace("Cube::onLoad");
		
		this.library.statusChangeEvent.addListener(this, this.StatusChanged);
		
		this._parent["player"].Initialize(this.library);
		
		Stage.addListener(this);
		
	}
	
	public function onResize():Void {
		trace("Stage.resize");
		this.currentView.Resize();
	}
		
	private function StatusChanged(status:String, library:core.Library):Void {
		trace("Cube::StatusChanged " + status);
	//	this["thelog"].text	+= "s:" + status + " " + this.library.GetLastError() + "\n";
		
		if (status == "authorized" && this.library.connected) {
			// Lets start sending queries
			
			// TEST
/*			var query:core.query.ListSelection	= new core.query.ListSelection();
			query.ListMetaKey("genre");
			
			this.library.SendQuery(query);
	*/
			this.currentView	= new cube.MetakeyLister(this.library, ["genre", "artist", "album"], this["list_container"]);
			this.currentView.Start();
			
		}
		
		if (status == "error" && this.retries>0) {
			this.retries--;
			this.library.Connect();
		}
	}
	
	public function SubmitForm(formMC:MovieClip):Void {
		this.library.host		= formMC["host"].text;
		this.library.queryPort	= int(formMC["queryport"].text);
		this.library.httpPort	= int(formMC["httpport"].text);
		this.library.username	= formMC["username"].text;
		this.library.password	= formMC["password"].text;
		
		if (this.saveData) {
			this.saveData.data.host		= formMC["host"].text;
			this.saveData.data.queryPort	= int(formMC["queryport"].text);
			this.saveData.data.httpPort	= int(formMC["httpport"].text);
			this.saveData.data.username	= formMC["username"].text;
			this.saveData.data.password	= formMC["password"].text;
			this.saveData.data.autologin= (formMC["autologin"]._currentframe==2);
			this.saveData.flush();
			trace("SubmitForm");
		}
		this._parent.gotoAndStop(2);		
		this.library.Connect();
	}
	
	public function LoadForm(formMC:MovieClip):Void {
		trace("LoadForm " + formMC);
		formMC["password"].password = true;
		this.saveData	= SharedObject.getLocal("mC2connection");
		this.saveData.form	= formMC;
		this.saveData.cube	= this;
		SharedObject.addListener("mC2connection", this.SOLoaded);
	}
	
	public function SOLoaded(so):Void {
		trace("SOLOADED " + so.form);
		var formMC	= so.form;
		if(so.data.host){ 			formMC["host"].text	= so.data.host; }
		if(so.data.queryPort){		formMC["queryport"].text=so.data.queryPort; }
		if(so.data.httpPort){		formMC["httpport"].text=so.data.httpPort; }
		if(so.data.username){		formMC["username"].text=so.data.username; }
		if(so.data.password){		formMC["password"].text=so.data.password; }

/*		formMC["host"].text	= "vallgraven.intermezzon.com";
//		formMC["host"].text	= "localhost";
		formMC["username"].text = "doep";
		formMC["password"].text = "doep";
	*/	
		if (so.data.autologin) {
			so.cube.SubmitForm(formMC);
		}
	}
	
}