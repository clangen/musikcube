//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "NonLibraryTrackHelper.h"
#include <boost/bind.hpp>
#include <core/plugin/PluginFactory.h>
#include <core/sdk/IMetadataReader.h>
#include <core/io/DataStreamFactory.h>

using namespace musik::core;

NonLibraryTrackHelper NonLibraryTrackHelper::sInstance;

NonLibraryTrackHelper::NonLibraryTrackHelper(void)
: threadIsRunning(false) {
}

NonLibraryTrackHelper::~NonLibraryTrackHelper(void) {
}

NonLibraryTrackHelper& NonLibraryTrackHelper::Instance() {
    return NonLibraryTrackHelper::sInstance;
}

void NonLibraryTrackHelper::ReadTrack(musik::core::TrackPtr track) {
    bool threadRunning = false;

    {
        boost::mutex::scoped_lock lock(this->mutex);
        this->tracksToRead.push_back(TrackWeakPtr(track));
        threadRunning = this->threadIsRunning;
    }

    if (!threadRunning) {
        if (this->helperThread) {
            this->helperThread->join();
        }

        this->helperThread.reset(new boost::thread(
            boost::bind(&NonLibraryTrackHelper::ThreadLoop,this)));
    }
}

void NonLibraryTrackHelper::ThreadLoop() {
    /* load all IMetadataReader plugins */
    typedef metadata::IMetadataReader PluginType;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;
    typedef std::vector<std::shared_ptr<metadata::IMetadataReader> > MetadataReaderList;

    MetadataReaderList metadataReaders =
        PluginFactory::Instance() .QueryInterface<PluginType, Deleter>("GetMetadataReader");

    bool moreTracks = true;

    while (moreTracks) {
        musik::core::TrackPtr track;

        /* pop the next track, if one exists. */
        {
            boost::mutex::scoped_lock lock(this->mutex);

            if (!this->tracksToRead.empty()) {
                track = this->tracksToRead.front().lock();
                this->tracksToRead.pop_front();
            }

            moreTracks = !this->tracksToRead.empty();

            if (!moreTracks) {
                this->threadIsRunning = false;
            }
        }

        if (track) {
            /* we only support local files. other URIs are ignored */
            if (musik::core::io::DataStreamFactory::IsLocalFileStream(track->URI().c_str())) {
                std::string url = track->URI();

                std::string::size_type lastDot = url.find_last_of(".");
                if (lastDot != std::string::npos) {
                    track->SetValue("extension", url.substr(lastDot + 1).c_str());
                }

                /* see if we can find a MetadataReader plugin that supports this file */
                typedef MetadataReaderList::iterator Iterator;
                Iterator it = metadataReaders.begin();
                while (it != metadataReaders.end()) {
                    if ((*it)->CanRead(track->GetValue("extension").c_str())) {
                        (*it)->Read(url.c_str(), track.get());
                        break;
                    }

                    it++;
                }

                this->TrackMetadataUpdated(track);
            }
        }
    }
}
