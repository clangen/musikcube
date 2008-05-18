// test_project.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <core/Library/LocalDB.h>
#include <core/Query/Lists.h>
#include <core/MetadataValue.h>

// The GenreListWindow is an example class that represent
// the visual list of genres
class GenreListWindow : public sigslot::has_slots<> {
	public:
		// The list should contain a vector to store the MetadataValues
		musik::core::MetadataValueVector genres;

		// The callback method when artists are retrieved
		void OnGernes(musik::core::MetadataValueVector *newGenres,bool clear);

		GenreListWindow();

		void SelectGenre(int position);
};



// The ArtistListWindow is an example class that represent
// the visual list of artists
class ArtistListWindow : public sigslot::has_slots<> {
	public:
		// The list should contain a vector to store the MetadataValues
		musik::core::MetadataValueVector artists;

		// The callback method when artists are retrieved
		void OnArtists(musik::core::MetadataValueVector *newArtists,bool clear);

		ArtistListWindow();

		void SelectArtist(int position);
};

// The ViewWindow is an example of a view that contain
// a genre list, a artist list and a list of tracks (not yet)
class ViewWindow{

	public:
		// The view has only one query, the Query::Lists
		musik::core::Query::Lists listQuery;

		GenreListWindow genreWindow;
		ArtistListWindow artistWindow;

		void SendQuery();

};

// For the ease of the example, we make the library and the view (ViewWindow) global
musik::core::Library::LocalDB library;
ViewWindow view;


/////////////////////////////////////////////////////////////
// Constructing the GenreListWindow
GenreListWindow::GenreListWindow(){
	// The constructor in this example connects the slots to the query
	view.listQuery.OnMetadataEvent("genre").connect(this,&GenreListWindow::OnGernes);
}

ArtistListWindow::ArtistListWindow(){
	// The constructor in this example connects the slots to the query
	view.listQuery.OnMetadataEvent("artist").connect(this,&ArtistListWindow::OnArtists);
}

void GenreListWindow::OnGernes(musik::core::MetadataValueVector *newGenres,bool clear){
	if(clear){
		// If the clear flag is set, erase all genres
		this->genres.clear();
	}
	// Append the incoming genres.
	this->genres.insert(this->genres.end(),newGenres->begin(),newGenres->end());
}

void ArtistListWindow::OnArtists(musik::core::MetadataValueVector *newArtists,bool clear){
	if(clear){
		// If the clear flag is set, erase all genres
		this->artists.clear();
	}
	// Append the incoming artists.
	this->artists.insert(this->artists.end(),newArtists->begin(),newArtists->end());
}


void ViewWindow::SendQuery(){

	// In this example I will send the QUERY_WAIT so that the AddQuery
	// will hold until the query is parsed. This is not the recommended way. See Library::Base::OnQueryQueueStart
	library.AddQuery(this->listQuery,QUERY_AUTOCALLBACK|QUERY_WAIT);
}

void GenreListWindow::SelectGenre(int position){
	// User selected position in the list
	// add selection to the query
	view.listQuery.SelectMetadata("genre",this->genres[position]->id); // should check if this exists first

	// Send the query to update the other lists
	view.SendQuery();
}

void ArtistListWindow::SelectArtist(int position){
	// User selected position in the list
	// add selection to the query
	view.listQuery.SelectMetadata("artist",this->artists[position]->id); // should check if this exists first

	// Send the query to update the other lists
	view.SendQuery();
}





int _tmain(int argc, _TCHAR* argv[]){

	// Start by starting up the library
	library.Startup();

	// Send an empty query to the library.
	// This will make the Query::Lists list all metadata that are connected (genres and artists in this case)
	view.SendQuery();

	// Lets assume that this could be a gui application
	// I added the SelectArtist and SelectGenre to the lists
	// to simulate that a user clicks on the genre/artist

	// Lets assume that the user selects the 3rd position in the genre list
	view.genreWindow.SelectGenre(3);
	// This will send the genre to the selection in the query and send the query to the library
	// Since the metakey "genre" is now selected, the Query::Lists will not query for
	// genres anymore and the callbacks for artists will only be called.

	system("PAUSE");

	return 0;
}

