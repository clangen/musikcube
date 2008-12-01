//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2007, Casey Langen
//
// Sources and Binaries of: mC2, win32cpp
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright 
//      notice, this list of conditions and the following disclaimer in the 
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may 
//      be used to endorse or promote products derived from this software 
//      without specific prior written permission. 
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE. 
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"
#include <cube/MainWindowController.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/TopLevelWindow.hpp>
#include <core/Common.h>
#include <core/PlaybackQueue.h>
#include <core/TrackFactory.h>

#include <boost/program_options.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prevInstance, LPTSTR commandLine, int showCommand)
{
    // Lets parse the input arguments
    {
        std::vector<utfstring> arguments    = boost::program_options::split_winmain(commandLine);
        arguments.push_back(utfstring(UTF("M:\\musik\\Electronic\\Depeche Mode\\Ultra\\01_Barrel of a gun.mp3")));
        if(arguments.size()){
            musik::core::tracklist::Ptr nowPlaying  = musik::core::PlaybackQueue::Instance().NowPlayingTracklist();
            if(nowPlaying){
                bool tracksAdded(false);
                for(std::vector<utfstring>::iterator uri=arguments.begin();uri!=arguments.end();++uri){
                    musik::core::TrackPtr newTrack  = musik::core::TrackFactory::CreateTrack(*uri);
                    if(newTrack){
                        (*nowPlaying) += newTrack;
                        tracksAdded = true;
                    }
                }
                if(tracksAdded){
                    musik::core::PlaybackQueue::Instance().Play();
                }
            }
        }
    }

/*    boost::program_options::wcommand_line_parser(
            boost::program_options::split_winmain(commandLine)
        );
*/


    // Initialize locale
    try {
		uistring appDirectory( musik::core::GetApplicationDirectory() );
        Locale::Instance()->SetLocaleDirectory(appDirectory + _T("\\locales"));
	    Locale::Instance()->LoadConfig(_T("english"));
    }
    catch(...) {
    }

    // Initialize the main application (mC2.exe)
    Application::Initialize(instance, prevInstance, commandLine, showCommand);

    // Create the main window and its controller
    TopLevelWindow mainWindow(_T("musikCube 2"));
    MainWindowController mainController(mainWindow);


    // Initialize and show the main window, and run the event loop.
    Application::Instance().Run(mainWindow);

    return 0;
}
