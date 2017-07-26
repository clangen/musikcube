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

#include "TaglibMetadataReader.h"

#ifdef WIN32
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
    #include <taglib/mp4/mp4file.h>
    #include <taglib/ogg/oggfile.h>
    #include <taglib/toolkit/tpropertymap.h>
#else
    #include <taglib/tlist.h>
    #include <taglib/tfile.h>
    #include <taglib/tag.h>
    #include <taglib/fileref.h>
    #include <taglib/audioproperties.h>
    #include <taglib/mpegfile.h>
    #include <taglib/id3v1tag.h>
    #include <taglib/id3v1genres.h>
    #include <taglib/id3v2tag.h>
    #include <taglib/id3v2header.h>
    #include <taglib/id3v2frame.h>
    #include <taglib/attachedpictureframe.h>
    #include <taglib/commentsframe.h>
    #include <taglib/mp4file.h>
    #include <taglib/oggfile.h>
    #include <taglib/tpropertymap.h>
#endif

#include <vector>
#include <string>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#ifdef WIN32
static inline std::wstring utf8to16(const char* utf8) {
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    wchar_t* buffer = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, size);
    std::wstring utf16fn(buffer);
    delete[] buffer;
    return utf16fn;
}
#endif

TaglibMetadataReader::TaglibMetadataReader() {
}

TaglibMetadataReader::~TaglibMetadataReader() {
}

void TaglibMetadataReader::Destroy() {
    delete this;
}

bool TaglibMetadataReader::CanRead(const char *extension){
    if (extension) {
        std::string ext(extension);
        boost::algorithm::to_lower(ext);

        return
            ext.compare("mp3") == 0 ||
            ext.compare("ogg") == 0 ||
            ext.compare("aac") == 0 ||
            ext.compare("m4a") == 0 ||
            ext.compare("flac") == 0 ||
            ext.compare("ape") == 0 ||
            ext.compare("mpc") == 0;
    }

    return false;
}

bool TaglibMetadataReader::Read(const char* uri, musik::core::sdk::ITrackWriter *track) {
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

#include <iostream>

bool TaglibMetadataReader::GetGenericTag(const char* uri, musik::core::sdk::ITrackWriter *target) {
#ifdef WIN32
    TagLib::FileRef file(utf8to16(uri).c_str());
#else
    TagLib::FileRef file(uri);
#endif

    if (!file.isNull()) {
        TagLib::Tag *tag = file.tag();

        if (tag) {
            if (!tag->title().isEmpty()) {
                this->SetTagValue("title", tag->title(), target);
            }
            else {
                this->SetTagValue("title", uri, target);
            }

            this->SetTagValue("album",tag->album(), target);
            this->SetSlashSeparatedValues("artist",tag->artist() , target);
            this->SetTagValue("genre",tag->genre(), target);
            this->SetTagValue("comment",tag->comment(), target);

            if (tag->track()) {
                this->SetTagValue("track", tag->track(), target);
            }

            if (tag->year()) {
                this->SetTagValue("year", tag->year(), target);
            }

            TagLib::PropertyMap map = tag->properties();
            if (map.contains("DISCNUMBER")) {
                TagLib::StringList value = map["DISCNUMBER"];
                if (value.size()) {
                    this->SetTagValue("disc", value[0], target);
                }
            }

            TagLib::AudioProperties *audio = file.audioProperties();
            this->SetAudioProperties(audio, target);

            return true;
        }
    }

    return false;
}

bool TaglibMetadataReader::GetID3v2Tag(const char* uri, musik::core::sdk::ITrackWriter *track) {
    TagLib::ID3v2::FrameFactory::instance()->setDefaultTextEncoding(TagLib::String::UTF8);

#ifdef WIN32
    TagLib::MPEG::File file(utf8to16(uri).c_str());
#else
    TagLib::MPEG::File file(uri);
#endif

    TagLib::ID3v2::Tag *id3v2 = file.ID3v2Tag();

    if (id3v2) {
        TagLib::AudioProperties *audio = file.audioProperties();
        TagLib::ID3v2::FrameListMap allTags = id3v2->frameListMap();

        if (!id3v2->title().isEmpty()) {
            this->SetTagValue("title", id3v2->title(), track);
        }

        this->SetTagValue("album", id3v2->album(), track);

        //{
        //    TagLib::Map<TagLib::ByteVector, TagLib::ID3v2::FrameList>::Iterator it = allTags.begin();
        //    while (it != allTags.end()) {
        //        const char* f = it->first.data();
        //        ++it;
        //    }
        //}

        /* year */

        if (!allTags["TYER"].isEmpty()) { /* ID3v2.3*/
            this->SetTagValue("year", allTags["TYER"].front()->toString().substr(0, 4), track);
        }

        if (!allTags["TDRC"].isEmpty()) { /* ID3v2.4*/
            this->SetTagValue("year", allTags["TDRC"].front()->toString().substr(0, 4), track);
        }

        if (!allTags["TCOP"].isEmpty()) { /* ID3v2.3*/
            this->SetTagValue("year", allTags["TCOP"].front()->toString().substr(0, 4), track);
        }

        /* TRCK is the track number (or "trackNum/totalTracks") */

        std::vector<std::string> splitTrack;
        if (!allTags["TRCK"].isEmpty()) {
            std::string tempTrack = allTags["TRCK"].front()->toString().toCString(true);
            boost::algorithm::split(splitTrack, tempTrack, boost::algorithm::is_any_of("/"));
            this->SetTagValue("track", splitTrack[0].c_str(), track);

            if (splitTrack.size() > 1) {
                this->SetTagValue("totaltracks", splitTrack[1].c_str(), track);
            }
        }

        this->SetTagValues("bpm", allTags["TBPM"], track);
        this->SetSlashSeparatedValues("composer", allTags["TCOM"], track);
        this->SetTagValues("copyright", allTags["TCOP"], track);
        this->SetTagValues("encoder", allTags["TENC"], track);
        this->SetTagValues("writer", allTags["TEXT"], track);
        this->SetTagValues("org.writer", allTags["TOLY"], track);
        this->SetSlashSeparatedValues("publisher", allTags["TPUB"], track);
        this->SetTagValues("mood", allTags["TMOO"], track);
        this->SetSlashSeparatedValues("org.artist", allTags["TOPE"], track);
        this->SetTagValues("language", allTags["TLAN"], track);
        this->SetTagValues("disc", allTags["TPOS"], track);
        this->SetTagValues("lyrics", allTags["USLT"], track);
        this->SetTagValues("disc", allTags["TPOS"], track);

        /* genre. note that multiple genres may be present */

        if (!allTags["TCON"].isEmpty()) {
            TagLib::ID3v2::FrameList genres = allTags["TCON"];

            TagLib::ID3v2::FrameList::ConstIterator it = genres.begin();

            for (; it != genres.end(); ++it) {
                TagLib::String genreString = (*it)->toString();

                if (!genreString.isEmpty()) {
                    /* note1: apparently genres will already be de-duped */
                    int numberLength = 0;
                    bool isNumber = true;

                    TagLib::String::ConstIterator charIt = genreString.begin();
                    for (; isNumber && charIt != genreString.end(); ++charIt) {
                        isNumber = (*charIt >= '0' && *charIt <= '9');

                        if (isNumber) {
                            ++numberLength;
                        }
                    }

                    if (isNumber) { /* old ID3v1 tags had numbers for genres. */
                        int genreNumber = genreString.toInt();
                        if (genreNumber >= 0 && genreNumber <= 255) {
                            genreString = TagLib::ID3v1::genre(genreNumber);
                        }
                    }
                    else {
                        if (numberLength > 0) { /* genre may start with a number. */
                            if (genreString.substr(numberLength, 1) == " ") {
                                int genreNumber = genreString.substr(0, numberLength).toInt();
                                if (genreNumber >= 0 && genreNumber <= 255) {
                                    this->SetTagValue("genre", TagLib::ID3v1::genre(genreNumber), track);
                                }

                                /* strip the number */
                                genreString = genreString.substr(numberLength + 1);
                            }
                        }

                        if (!genreString.isEmpty()) {
                            this->SetTagValue("genre", genreString, track);
                        }
                    }
                }
            }
        }

        /* artists */

        this->SetSlashSeparatedValues("artist" ,allTags["TPE1"], track);
        this->SetSlashSeparatedValues("album_artist", allTags["TPE2"], track);
        this->SetSlashSeparatedValues("conductor", allTags["TPE3"], track);
        this->SetSlashSeparatedValues("interpreted", allTags["TPE4"], track);

        /* audio properties include things like bitrate, channels, and duration */

        this->SetAudioProperties(audio, track);

        /* comments, mood, and rating */

        TagLib::ID3v2::FrameList comments = allTags["COMM"];

        TagLib::ID3v2::FrameList::Iterator it = comments.begin();
        for ( ; it != comments.end(); ++it) {
            TagLib::ID3v2::CommentsFrame *comment
                = dynamic_cast<TagLib::ID3v2::CommentsFrame*> (*it);

            TagLib::String temp    = comment->description();
            std::string description(temp.begin(), temp.end());

            if (description.empty()) {
                this->SetTagValue("comment", comment->toString(), track);
            }
            else if (description.compare("MusicMatch_Mood") == 0) {
                this->SetTagValue("mood", comment->toString(), track);
            }
            else if (description.compare("MusicMatch_Preference") == 0) {
                this->SetTagValue("textrating", comment->toString(), track);
            }
        }

        /* thumbnail */

        TagLib::ID3v2::FrameList pictures = allTags["APIC"];
        if(!pictures.isEmpty()) {
            /* there can be multiple pictures, apparently. let's just use
            the first one. */

            TagLib::ID3v2::AttachedPictureFrame *picture =
                static_cast<TagLib::ID3v2::AttachedPictureFrame*>(pictures.front());

            TagLib::ByteVector pictureData = picture->picture();
            long long size = pictureData.size();

            if(size > 32) {    /* noticed that some id3tags have like a 4-8 byte size with no thumbnail */
                track->SetThumbnail(pictureData.data(), size);
            }
        }

        return true;
    }

    return false;
}

void TaglibMetadataReader::SetTagValue(
    const char* key,
    const TagLib::String tagString,
    musik::core::sdk::ITrackWriter *track)
{
    std::string value(tagString.to8Bit(true));
    track->SetValue(key, value.c_str());
}

void TaglibMetadataReader::SetTagValue(
    const char* key,
    const char* string,
    musik::core::sdk::ITrackWriter *track)
{
    std::string temp(string);
    track->SetValue(key, temp.c_str());
}

void TaglibMetadataReader::SetTagValue(
    const char* key,
    const int tagInt,
    musik::core::sdk::ITrackWriter *target)
{
    std::string temp = boost::str(boost::format("%1%") % tagInt);
    target->SetValue(key, temp.c_str());
}

void TaglibMetadataReader::SetTagValues(
    const char* key,
    const TagLib::ID3v2::FrameList &frame,
    musik::core::sdk::ITrackWriter *target)
{
    if (!frame.isEmpty()) {
        TagLib::ID3v2::FrameList::ConstIterator value = frame.begin();

        for ( ; value != frame.end(); ++value) {
            TagLib::String tagString = (*value)->toString();
            if(!tagString.isEmpty()) {
                std::string value(tagString.to8Bit(true));
                target->SetValue(key,value.c_str());
            }
        }
    }
}

void TaglibMetadataReader::SetSlashSeparatedValues(
    const char* key,
    TagLib::String tagString,
    musik::core::sdk::ITrackWriter *track)
{
    if(!tagString.isEmpty()) {
        std::string value(tagString.to8Bit(true));
        std::vector<std::string> splitValues;

        boost::algorithm::split(splitValues, value, boost::algorithm::is_any_of("/"));

        std::vector<std::string>::iterator it = splitValues.begin();
        for( ; it != splitValues.end(); ++it) {
            track->SetValue(key, it->c_str());
        }
    }
}

void TaglibMetadataReader::SetSlashSeparatedValues(
    const char* key,
    const TagLib::ID3v2::FrameList &frame,
    musik::core::sdk::ITrackWriter *track)
{
    if(!frame.isEmpty()) {
        TagLib::ID3v2::FrameList::ConstIterator value = frame.begin();
        for ( ; value != frame.end(); ++value) {
            TagLib::String tagString = (*value)->toString();
            this->SetSlashSeparatedValues(key, tagString, track);
        }
    }
}

void TaglibMetadataReader::SetAudioProperties(
    TagLib::AudioProperties *audioProperties,
    musik::core::sdk::ITrackWriter *track)
{
    /* FIXME: it's overkill to bring boost in just to convert ints to strings */

    if (audioProperties) {
        std::string duration = boost::str(boost::format("%1%") % audioProperties->length());
        this->SetTagValue("duration", duration, track);

        int bitrate = audioProperties->bitrate();

        if(bitrate) {
            std::string temp(boost::str(boost::format("%1%") % bitrate));
            this->SetTagValue("bitrate", temp, track);
        }

        int channels = audioProperties->channels();

        if(channels) {
            std::string temp(boost::str(boost::format("%1%") % channels));
            this->SetTagValue("channels", temp, track);
        }
    }
}
