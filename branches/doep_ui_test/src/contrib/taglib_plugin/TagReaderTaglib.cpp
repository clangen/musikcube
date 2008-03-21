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

#include "TagReaderTaglib.h"

#include <tlist.h>
#include <fileref.h>
#include <tfile.h>
#include <tag.h>
#include <mpegfile.h>
#include <id3v2tag.h>
#include <id3v2frame.h>
#include <id3v2header.h>
#include <id3v1tag.h>
#include <id3v1genres.h>
#include <audioproperties.h>
#include <attachedpictureframe.h>
#include <commentsframe.h>

#include <vector>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>


// Create a instance

TagReaderTaglib::TagReaderTaglib(void){
}

TagReaderTaglib::~TagReaderTaglib(void){
}

void TagReaderTaglib::Destroy() {
    // do nothing! we use a static reference for every call to GetMetaDataReader()
    delete this;
}

bool TagReaderTaglib::CanReadTag(const wchar_t *extension){

    if(extension){
//	    std::wstring ext(extension);
        if(	std::wstring(_T("mp3"))==extension ) {
		    return true;
	    }
    }

	return false;

}


bool TagReaderTaglib::ReadTag(musik::core::Track *track){

	TagLib::ID3v2::FrameFactory::instance()->setDefaultTextEncoding(TagLib::String::UTF16);

    if(std::wstring(_T("mp3"))==track->GetValue("extension")){
        TagLib::MPEG::File oFile(track->GetValue("path"));
		TagLib::ID3v2::Tag *oTagv2	= oFile.ID3v2Tag();
		TagLib::AudioProperties *oAudioProperties	= oFile.audioProperties();
		if(oTagv2){
			// Successfully found ID3V2-tag

			//////////////////////////////////////////////////////////////////////////////
			// Get ALL tags in a map
			TagLib::ID3v2::FrameListMap aAllTags	= oTagv2->frameListMap();

			if(!oTagv2->title().isEmpty()){
				this->SetTagValue("title",oTagv2->title(),track);
			}else{
				this->SetTagValue("title",track->GetValue("filename"),track);
			}
			this->SetTagValue("album",oTagv2->album(),track);

			//////////////////////////////////////////////////////////////////////////////
			// YEAR
			if( !aAllTags["TYER"].isEmpty() )		// ID3 2.3
				this->SetTagValue("year",aAllTags["TYER"].front()->toString().substr(0,4),track);
			if( !aAllTags["TDRC"].isEmpty() )		// ID3 2.4
				this->SetTagValue("year",aAllTags["TDRC"].front()->toString().substr(0,4),track);


			// TODO: Handle the "totaltracks" part of the TRCK
			this->SetTagValues("track",aAllTags["TRCK"],track);

			this->SetTagValues("bpm",aAllTags["TBPM"],track);
			this->SetSlashSeparatedValues("composer",aAllTags["TCOM"],track);
			this->SetTagValues("copyright",aAllTags["TCOP"],track);
			this->SetTagValues("encoder",aAllTags["TENC"],track);
			this->SetTagValues("writer",aAllTags["TEXT"],track);
			this->SetTagValues("org.writer",aAllTags["TOLY"],track);
			this->SetSlashSeparatedValues("publisher",aAllTags["TPUB"],track);
			this->SetTagValues("mood",aAllTags["TMOO"],track);
			this->SetSlashSeparatedValues("org.artist",aAllTags["TOPE"],track);
			this->SetTagValues("language",aAllTags["TLAN"],track);
			this->SetTagValues("disc",aAllTags["TPOS"],track);

			//////////////////////////////////////////////////////////////////////////////
			// GENRES
			if(!aAllTags["TCON"].isEmpty()){
				TagLib::ID3v2::FrameList genres	= aAllTags["TCON"];
				for(TagLib::ID3v2::FrameList::ConstIterator genreFrame=genres.begin();genreFrame!=genres.end();++genreFrame){
					TagLib::String genreString	= (*genreFrame)->toString();
					if( !genreString.isEmpty() ){
						// There is no need to worry about duplicates, since the Track will not save duplicates.
						// TODO: This should also handle old ID3v1 genre numbers
                        bool isNumber(true);
                        for(TagLib::String::ConstIterator charIt=genreString.begin();isNumber && charIt!=genreString.end();++charIt){
                            isNumber = *charIt >= '0' && *charIt <= '9';
                        }

                        if(isNumber){
                            int genreNumber = genreString.toInt();
                            if(genreNumber>= 0 && genreNumber<= 255)
                                genreString = TagLib::ID3v1::genre(genreNumber);
                        }
    				    this->SetTagValue("genre",genreString,track);

                    }
				}
			}

			//////////////////////////////////////////////////////////////////////////////
			// ARTISTS
			this->SetSlashSeparatedValues("artist",aAllTags["TPE1"],track);
			this->SetSlashSeparatedValues("artist",aAllTags["TPE2"],track);
			this->SetSlashSeparatedValues("conductor",aAllTags["TPE3"],track);
			this->SetSlashSeparatedValues("interpreted",aAllTags["TPE4"],track);

			//////////////////////////////////////////////////////////////////////////////
			// Audio properties
			if(oAudioProperties){
				std::wstring duration	= boost::str(boost::wformat(_T("%1%"))%oAudioProperties->length());
				this->SetTagValue("duration",duration,track);
			}

			//////////////////////////////////////////////////////////////////////////////
			// Comments
			TagLib::ID3v2::FrameList	comments	= aAllTags["COMM"];
			for(TagLib::ID3v2::FrameList::Iterator commentFrame=comments.begin();commentFrame!=comments.end();++commentFrame){
				TagLib::ID3v2::CommentsFrame *comment	= dynamic_cast<TagLib::ID3v2::CommentsFrame*> (*commentFrame);

				TagLib::String temp	= comment->description();
				std::wstring description(temp.begin(),temp.end());

				if(description.empty()){
					this->SetTagValue("comment",comment->toString(),track);
				}else if(description==_T("MusicMatch_Mood")){
					this->SetTagValue("mood",comment->toString(),track);
				}else if(description==_T("MusicMatch_Preference")){
					this->SetTagValue("textrating",comment->toString(),track);
				}
			}

			//////////////////////////////////////////////////////////////////////////////
			// Thumbnail
			TagLib::ID3v2::FrameList pictures	= aAllTags["APIC"];
			if(!pictures.isEmpty()){
				// Thumbnail exists
				// Just get the front() picture
				TagLib::ID3v2::AttachedPictureFrame *picture = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(pictures.front());
				TagLib::ByteVector pictureData	= picture->picture();
				DBINT size	= pictureData.size();

				if(size>32){	// Noticed that some id3tags have like a 4-8 byte size with no thumbnail
					char *data	= pictureData.data();
					track->SetThumbnail(data,size);
				}

			}


			return true;
		}
	}

	// Try the "generic" way
/*	TagLib::FileRef	oFile(oMedia->sFileFullPath.c_str());
	TagLib::Tag		*oTag	= oFile.tag();
	TagLib::AudioProperties *oAudioProperties	= oFile.audioProperties();
	if(oTag && oAudioProperties){
		// Successfully found tags
		return this->getStandardTags(oMedia,oTag,oAudioProperties);
	}

	//oFile.audioProperties();
*/
	return false;
}

void TagReaderTaglib::SetTagValue(const char* key,const TagLib::String tagString,musik::core::Track *track){
	using namespace musik::core;
	std::wstring value(tagString.begin(),tagString.end());
    track->SetValue(key,value.c_str());
}

void TagReaderTaglib::SetTagValue(const char* key,const wchar_t* string,musik::core::Track *track){
	using namespace musik::core;
	track->SetValue(key,string);
}


void TagReaderTaglib::SetTagValues(const char* key,const TagLib::ID3v2::FrameList &frame,musik::core::Track *track){
	using namespace musik::core;
	if(!frame.isEmpty()){
		for(TagLib::ID3v2::FrameList::ConstIterator value=frame.begin();value!=frame.end();++value){
			TagLib::String tagString	= (*value)->toString();
			if( !tagString.isEmpty() ){
				std::wstring value(tagString.begin(),tagString.end());
                track->SetValue(key,value.c_str());
			}
		}
	}
}

/*
bool TagReaderTaglib::getStandardTags(musik::core::Track &track,TagLib::Tag *oTag,TagLib::AudioProperties *oAudioProperties){
	oMedia->setArtist(oTag->artist().toCString(false));
	oMedia->setAlbum(oTag->album().toCString(false));
	oMedia->setGenre(oTag->genre().toCString(false));
	oMedia->setTitle(oTag->title().toCString(false));
	oMedia->iTrack		= oTag->track();
	oMedia->iYear		= oTag->year();
	oMedia->setComment(oTag->comment().toCString(false));

	if(oAudioProperties){
		oMedia->iDuration	= oAudioProperties->length();
	}

	return true;
}
*/
void TagReaderTaglib::SetSlashSeparatedValues(const char* key,const TagLib::ID3v2::FrameList &frame,musik::core::Track *track){
	using namespace musik::core;
	if(!frame.isEmpty()){
		for(TagLib::ID3v2::FrameList::ConstIterator frameValue=frame.begin();frameValue!=frame.end();++frameValue){
			TagLib::String tagString	= (*frameValue)->toString();
			if( !tagString.isEmpty() ){
				std::wstring value(tagString.begin(),tagString.end());
				std::vector<std::wstring> splitValues;

				boost::algorithm::split(splitValues,value,boost::algorithm::is_any_of(_T("/")));

				for(std::vector<std::wstring>::iterator theValue=splitValues.begin();theValue!=splitValues.end();++theValue){
                    track->SetValue(key,theValue->c_str());
				}
			}
		}
	}
}


