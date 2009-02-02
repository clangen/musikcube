//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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
#include <cube/CommandlineParser.h>

#include <core/PlaybackQueue.h>
#include <core/TrackFactory.h>
#include <boost/program_options.hpp>
#include <vector>

//////////////////////////////////////////////////////////////////////////////
using namespace musik::cube;
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///Parses the commands send through commandline
//////////////////////////////////////////
void CommandlineParser::ParseCommands(utfchar *commandLine){

    // arguments is a splitted list of files
    std::vector<utfstring> arguments    = boost::program_options::split_winmain(commandLine);

    if(arguments.size()){

        // Get the "now playing" tracklist
        musik::core::tracklist::Ptr nowPlaying  = musik::core::PlaybackQueue::Instance().NowPlayingTracklist();

        if(nowPlaying){
            bool tracksAdded(false);

            // Loop through the tracks and add to the "now playing"
            for(std::vector<utfstring>::iterator uri=arguments.begin();uri!=arguments.end();++uri){
                musik::core::TrackPtr newTrack  = musik::core::TrackFactory::CreateTrack(*uri);
                if(newTrack){
                    (*nowPlaying) += newTrack;
                    tracksAdded = true;
                }
            }
            if(tracksAdded){
                // If tracks has been added through commandline, start playing them directly
                musik::core::PlaybackQueue::Instance().Play();
            }
        }
    }

}

