//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#pragma once

#include "config.h"

#pragma warning(push, 0)
#include <taglib/tlist.h>
#include <taglib/tfile.h>
#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4file.h>
#pragma warning(pop)

#include <set>

#include "MetadataWriter.h"

class TaglibMetadataReader {
    public:
        TaglibMetadataReader();
        ~TaglibMetadataReader();

        bool Read(const char *uri, MetadataWriter *target);
        bool CanRead(const char *extension);

    private:
        template <typename T> void ReadFromMap(
            const T& map,
            MetadataWriter *target);

        template <typename T> void ExtractValueForKey(
            const T& map,
            const std::string& inputKey,
            const std::string& outputKey,
            MetadataWriter *target);

        template <typename T> std::string ExtractValueForKey(
            const T& map,
            const std::string& inputKey,
            const std::string& defaultValue);

        template <typename T> void ExtractReplayGain(
            const T& map,
            MetadataWriter *target);

        template<typename T> void ReadBasicData(
            const T* tag,
            const char* uri,
            MetadataWriter *target);

        void ExtractValueForKey(
            const TagLib::MP4::ItemMap& map,
            const std::string& inputKey,
            const std::string& outputKey,
            MetadataWriter *target);

        std::string ExtractValueForKey(
            const TagLib::MP4::ItemMap& map,
            const std::string& inputKey,
            const std::string& defaultValue);

        int ExtractRatingFromPopularimeter(
            const TagLib::ID3v2::FrameList& frame);

        void SetTagValueWithPossibleTotal(
            const std::string& value,
            const std::string& valueKey,
            const std::string& totalKey,
            MetadataWriter* track
        );

        void SetTagValue(
            const char* key,
            const char* string,
            MetadataWriter *target);

        void SetTagValue(
            const char* key,
            const TagLib::String tagString,
            MetadataWriter *target);

        void SetTagValue(
            const char* key,
            const int tagInt,
            MetadataWriter *target);

        void SetTagValues(const char* key,
            const TagLib::ID3v2::FrameList &frame,
            MetadataWriter *target);

        void SetAudioProperties(
            TagLib::AudioProperties *audioProperties,
            MetadataWriter *target);

        void SetSlashSeparatedValues(
            const char* key,
            const TagLib::ID3v2::FrameList &frame,
            MetadataWriter *target);

        void SetSlashSeparatedValues(
            const char* key,
            TagLib::String tagString,
            MetadataWriter *target);

        bool ReadID3V2(
            const char* uri,
            MetadataWriter *target);

        bool ReadID3V2(
            TagLib::ID3v2::Tag *tag,
            MetadataWriter *target);

        bool ReadGeneric(
            const char* uri,
            const std::string& extension,
            MetadataWriter *target);
};
