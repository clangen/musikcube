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

#include <boost/progress.hpp>
#include <boost/format.hpp>


int _tmain(int argc, _TCHAR* argv[])
{
    using namespace musik::core;

	Library::LocalDB library;

	library.Startup();

    library.indexer.AddPath(_T("U:/"));

    {
        boost::progress_timer timer;

        Sleep(2000);
        while(!library.GetInfo().empty()){
            std::wcout << library.GetInfo() << _T("\r");
            Sleep(100);
        }

        std::cout << std::endl;

    }

    system("PAUSE");

	return 0;
}

