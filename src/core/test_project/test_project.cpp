// test_project.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <stdlib.h>

#include <core/Library/LocalDB.h>
#include <core/Query/ListSelection.h>
#include <core/tracklist/Standard.h>

//#include <boost/progress.hpp>
#include <boost/format.hpp>


class Metalist : public sigslot::has_slots<>{
	public:
		std::string metakey;

		void OnMetaKey(musik::core::MetadataValueVector *metaValues,bool clear){
/*			if(this->metakey=="artist"){
				std::cout << std::endl << "key " << this->metakey << ": " << std::endl;
				for(musik::core::MetadataValueVector::iterator metaValue=metaValues->begin();metaValue!=metaValues->end();++metaValue){
					std::wcout << _T("    ") << (*metaValue)->id << _T(" ") << (*metaValue)->value << std::endl;
				}
			}*/
			std::cout << this->metakey << ": " << metaValues->size() << std::endl;
		};

		Metalist(std::string metakey,musik::core::Query::ListSelection &query){
			this->metakey	= metakey;
			query.OnMetadataEvent(this->metakey.c_str()).connect(this,&Metalist::OnMetaKey);
		};
};

int _tmain(int argc, _TCHAR* argv[])
{
    using namespace musik::core;

	Library::LocalDB library;

	library.Startup();

    Query::ListSelection query;

	Metalist genres("genre",query);
	Metalist artists("artist",query);
	Metalist albums("album",query);
	Metalist years("year",query);

    musik::core::tracklist::Standard tracklist;
    tracklist.Init(&library);
    tracklist.ConnectToQuery(query);


	{
//		boost::progress_timer timer;
        library.AddQuery(query,Query::Wait|Query::AutoCallback);
	}

    // Simulate a timer :)
    for(int i=0;i<2000;++i){
        library.RunCallbacks();
        Sleep(1);
    }
    for(int i=0;i<10;++i){
//        std::wcout << tracklist[i]->GetValue("artist");
  //      std::wcout << tracklist[i]->GetValue("album");
        std::wcout << tracklist[i]->GetValue("title") << std::endl;
    }


/*	{
		query.SelectMetadata("genre",6);
		boost::progress_timer timer;
		library.AddQuery(query,Query::Wait|Query::AutoCallback);
	}
	{
		query.SelectMetadata("artist",165);
		boost::progress_timer timer;
		library.AddQuery(query,Query::Wait|Query::AutoCallback);
	}
	{
		query.SelectMetadata("genre",5);
		query.SelectMetadata("genre",7);
		query.SelectMetadata("genre",8);
		query.SelectMetadata("genre",9);
		query.SelectMetadata("genre",10);
		boost::progress_timer timer;
		library.AddQuery(query,Query::Wait|Query::AutoCallback);
	}*/

/*
    library.indexer.AddPath(_T("D:/mp3/"));
    Sleep(2000);
    while(!library.GetInfo().empty()){
        std::wcout << library.GetInfo() << _T("\r");
        Sleep(100);
    }
*/
	system("PAUSE");

	return 0;
}

