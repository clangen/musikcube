//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Daniel �nnerby
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

#include "TagReaderTaglib.h"

#ifndef _HAVE_TAGLIB
#include <toolkit/tlist.h>
#include <toolkit/tfile.h>

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>

#include <mpeg/mpegfile.h>

#include <mpeg/id3v1/id3v1tag.h>
#include <mpeg/id3v1/id3v1genres.h>

#include <mpeg/id3v2/id3v2tag.h>
#include <mpeg/id3v2/id3v2header.h>
#include <mpeg/id3v2/id3v2frame.h>
#include <mpeg/id3v2/frames/attachedpictureframe.h>
#include <mpeg/id3v2/frames/commentsframe.h>

#include <taglib/ogg/oggfile.h>
#else //_HAVE_TAGLIB
#include <taglib/toolkit/tlist.h>
#include <taglib/toolkit/tfile.h>

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>

#include <taglib/mpeg/mpegfile.h>

#include <taglib/mpeg/id3v1/id3v1tag.h>
#include <taglib/mpeg/id3v1/id3v1genres.h>

#include <taglib/mpeg/id3v2/id3v2tag.h>
#include <taglib/mpeg/id3v2/id3v2header.h>
#include <taglib/mpeg/id3v2/id3v2frame.h>
#include <taglib/mpeg/id3v2/frames/attachedpictureframe.h>
#include <taglib/mpeg/id3v2/frames/commentsframe.h>

#include <taglib/ogg/oggfile.h>
#endif //_HAVE_TAGLIB

#include <vector>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


TagReaderTaglib::TagReaderTaglib() {
}

TagReaderTaglib::~TagReaderTaglib() {
}

void TagReaderTaglib::Destroy() {
    delete this;
}

bool TagReaderTaglib::CanReadTag(const char *extension){
    if (extension) {
		string ext(extension);
        boost::algorithm::to_lower(ext);

        return
            ext.compare("mp3") == 0 ||
            ext.compare("ogg") == 0 ||
            ext.compare("flac") == 0 ||
            ext.compare("ape") == 0 ||
            ext.compare("mpc") == 0;
    }

	return false;
}

bool TagReaderTaglib::ReadTag(const char* uri, musik::core::ITrack *track) {
    std::string path(uri);
    std::string extension;

    std::string::size_type lastDot = path.find_last_of(".");
    if (lastDot != std::string::npos) {
        extension = path.substr(lastDot + 1).c_str();
    }

    if (extension.size()) {
        boost::algorithm::to_lower(extension);

        if (extension == "mp3") {
            this->GetID3v2Tag(uri, track);
        }
    }

    return this->GetGenericTag(uri, track);
}

bool TagReaderTaglib::GetGenericTag(const char* uri, musik::core::ITrack *track) {
    TagLib::FileRef file(uri);

    if(!file.isNull()) {
        TagLib::Tag *tag = file.tag();

        if(tag) {

            if (!tag->title().isEmpty()) {
			    this->SetTagValue("title", tag->title(), track);
		    }
            else {
			    this->SetTagValue("title", uri, track);
		    }

		    this->SetTagValue("album",tag->album(), track);
            this->SetSlashSeparatedValues("artist",tag->artist() ,track);
            this->SetTagValue("genre",tag->genre(), track);
            this->SetTagValue("comment",tag->comment(), track);

            if (tag->track()) {
                this->SetTagValue("track", tag->track(), track);
            }

            if (tag->year()) {
                this->SetTagValue("year", tag->year(), track);
            }

            TagLib::AudioProperties *audio = file.audioProperties();
            this->SetAudioProperties(audio,track);

            return true;
        }
    }

    return false;
}

bool TagReaderTaglib::GetID3v2Tag(const char* uri, musik::core::ITrack *track){
    TagLib::ID3v2::FrameFactory::instance()->setDefaultTextEncoding(TagLib::String::UTF8);

    TagLib::MPEG::File file(uri);
	TagLib::ID3v2::Tag *id3v2 = file.ID3v2Tag();

	if (id3v2) {
        TagLib::AudioProperties *audio = file.audioProperties();
        TagLib::ID3v2::FrameListMap allTags = id3v2->frameListMap();

		if(!id3v2->title().isEmpty()) {
			this->SetTagValue("title",id3v2->title(), track);
		}

		this->SetTagValue("album",id3v2->album(), track);

		//////////////////////////////////////////////////////////////////////////////
		// YEAR
		if( !allTags["TYER"].isEmpty() )		// ID3 2.3
			this->SetTagValue("year",allTags["TYER"].front()->toString().substr(0,4),track);
		if( !allTags["TDRC"].isEmpty() )		// ID3 2.4
			this->SetTagValue("year",allTags["TDRC"].front()->toString().substr(0,4),track);


        // Split TRCK to track and totaltracks
        std::vector<string> splitTrack;
        if(!allTags["TRCK"].isEmpty()){
        	TagLib::wstring tempTrack = allTags["TRCK"].front()->toString().toWString();
            boost::algorithm::split(splitTrack,tempTrack,boost::algorithm::is_any_of("/"));
            this->SetTagValue("track",splitTrack[0].c_str(),track);
            if(splitTrack.size()>1){
                this->SetTagValue("totaltracks",splitTrack[1].c_str(),track);
            }
        }

		this->SetTagValues("bpm",allTags["TBPM"],track);
		this->SetSlashSeparatedValues("composer",allTags["TCOM"],track);
		this->SetTagValues("copyright",allTags["TCOP"],track);
		this->SetTagValues("encoder",allTags["TENC"],track);
		this->SetTagValues("writer",allTags["TEXT"],track);
		this->SetTagValues("org.writer",allTags["TOLY"],track);
		this->SetSlashSeparatedValues("publisher",allTags["TPUB"],track);
		this->SetTagValues("mood",allTags["TMOO"],track);
		this->SetSlashSeparatedValues("org.artist",allTags["TOPE"],track);
		this->SetTagValues("language",allTags["TLAN"],track);
		this->SetTagValues("disc",allTags["TPOS"],track);
		this->SetTagValues("lyrics",allTags["USLT"],track);

		//////////////////////////////////////////////////////////////////////////////
		// GENRES
		if(!allTags["TCON"].isEmpty()){
			TagLib::ID3v2::FrameList genres	= allTags["TCON"];
			for(TagLib::ID3v2::FrameList::ConstIterator genreFrame=genres.begin();genreFrame!=genres.end();++genreFrame){
				TagLib::String genreString	= (*genreFrame)->toString();
				if( !genreString.isEmpty() ){
					// There is no need to worry about duplicates, since the Track will not save duplicates.
					// TODO: This should also handle old ID3v1 genre numbers
                    int numberLength(0);
                    bool isNumber(true);
                    for(TagLib::String::ConstIterator charIt=genreString.begin();isNumber && charIt!=genreString.end();++charIt){
                        isNumber = *charIt >= '0' && *charIt <= '9';
                        if(isNumber){
                            ++numberLength;
                        }
                    }

                    if(!isNumber && numberLength>0){
                        // Genre starting with a number
                        if(genreString.substr(numberLength,1)==" "){
                            // If the genre number is appended by a space, then this is a id3v1 genre number
                            int genreNumber = genreString.substr(0,numberLength).toInt();
                            if(genreNumber>= 0 && genreNumber<= 255)
                                this->SetTagValue("genre",TagLib::ID3v1::genre(genreNumber),track);

                            // Remove the prepending genre number
                            genreString = genreString.substr(numberLength+1);
                        }
                    }

                    if(isNumber){
                        // If the genre is only a number, look for id3v1 genres
                        int genreNumber = genreString.toInt();
                        if(genreNumber>= 0 && genreNumber<= 255)
                            genreString = TagLib::ID3v1::genre(genreNumber);
                    }

    				if( !genreString.isEmpty() ){
	    			    this->SetTagValue("genre",genreString,track);
                    }

                }
			}
		}

		//////////////////////////////////////////////////////////////////////////////
		// ARTISTS
		this->SetSlashSeparatedValues("artist",allTags["TPE1"],track);
		this->SetSlashSeparatedValues("artist",allTags["TPE2"],track);
		this->SetSlashSeparatedValues("conductor",allTags["TPE3"],track);
		this->SetSlashSeparatedValues("interpreted",allTags["TPE4"],track);

		//////////////////////////////////////////////////////////////////////////////
		// Audio properties
        this->SetAudioProperties(audio,track);

		//////////////////////////////////////////////////////////////////////////////
		// Comments
		TagLib::ID3v2::FrameList	comments	= allTags["COMM"];
		for(TagLib::ID3v2::FrameList::Iterator commentFrame=comments.begin();commentFrame!=comments.end();++commentFrame){
			TagLib::ID3v2::CommentsFrame *comment	= dynamic_cast<TagLib::ID3v2::CommentsFrame*> (*commentFrame);

			TagLib::String temp	= comment->description();
			string description(temp.begin(),temp.end());

			if(description.empty()){
				this->SetTagValue("comment",comment->toString(),track);
			}else if(description.compare("MusicMatch_Mood") == 0){
				this->SetTagValue("mood",comment->toString(),track);
			}else if(description.compare("MusicMatch_Preference") == 0){
				this->SetTagValue("textrating",comment->toString(),track);
			}
		}

		//////////////////////////////////////////////////////////////////////////////
		// Thumbnail
		TagLib::ID3v2::FrameList pictures	= allTags["APIC"];
		if(!pictures.isEmpty()){
			// Thumbnail exists
			// Just get the front() picture
			TagLib::ID3v2::AttachedPictureFrame *picture = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(pictures.front());
			TagLib::ByteVector pictureData	= picture->picture();
			DBID size	= pictureData.size();

			if(size>32){	// Noticed that some id3tags have like a 4-8 byte size with no thumbnail
				char *data	= pictureData.data();
				track->SetThumbnail(data,size);
			}

		}


		return true;
	}

	return false;
}

void TagReaderTaglib::SetTagValue(const char* key,const TagLib::String tagString,musik::core::ITrack *track){
	using namespace musik::core;
	std::string value(tagString.begin(),tagString.end());
    track->SetValue(key, value.c_str());
}

void TagReaderTaglib::SetTagValue(const char* key,const char* string,musik::core::ITrack *track){
	using namespace musik::core;
	std::string temp (string);
	track->SetValue(key,temp.c_str());
}

void TagReaderTaglib::SetTagValue(const char* key,const int tagInt,musik::core::ITrack *track){
	string temp = boost::str(boost::format("%1%")%tagInt);
    track->SetValue(key,temp.c_str());
}

void TagReaderTaglib::SetTagValues(const char* key,const TagLib::ID3v2::FrameList &frame,musik::core::ITrack *track){
	using namespace musik::core;
	if(!frame.isEmpty()){
		for(TagLib::ID3v2::FrameList::ConstIterator value=frame.begin();value!=frame.end();++value){
			TagLib::String tagString = (*value)->toString();
			if( !tagString.isEmpty() ){
				std::string value(tagString.begin(),tagString.end());
                track->SetValue(key,value.c_str());
			}
		}
	}
}

void TagReaderTaglib::SetSlashSeparatedValues(const char* key,TagLib::String tagString,musik::core::ITrack *track){
	if( !tagString.isEmpty() ){
		string value(tagString.begin(),tagString.end());
		std::vector<string> splitValues;

		boost::algorithm::split(splitValues,value,boost::algorithm::is_any_of("/"));

		for(std::vector<string>::iterator theValue=splitValues.begin();theValue!=splitValues.end();++theValue){
            track->SetValue(key,theValue->c_str());
		}
	}
}

void TagReaderTaglib::SetSlashSeparatedValues(const char* key,const TagLib::ID3v2::FrameList &frame,musik::core::ITrack *track){
	using namespace musik::core;
	if(!frame.isEmpty()){
		for(TagLib::ID3v2::FrameList::ConstIterator frameValue=frame.begin();frameValue!=frame.end();++frameValue){
			TagLib::String tagString	= (*frameValue)->toString();
            this->SetSlashSeparatedValues(key,tagString,track);
		}
	}
}

void TagReaderTaglib::SetAudioProperties(TagLib::AudioProperties *audioProperties,musik::core::ITrack *track){
	if(audioProperties){
		string duration	= boost::str(boost::format("%1%")%audioProperties->length());

		this->SetTagValue("duration",duration,track);

        int bitrate( audioProperties->bitrate() );
        if(bitrate){
			string temp( boost::str(boost::format("%1%")%bitrate ) );
			this->SetTagValue("bitrate",temp,track);
        }

        int channels( audioProperties->channels() );
        if(channels){
			string temp( boost::str(boost::format("%1%")%channels ) );
			this->SetTagValue("channels",temp,track);
        }
	}
}


