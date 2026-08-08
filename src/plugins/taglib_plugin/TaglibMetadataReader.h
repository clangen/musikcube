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

/// @file TaglibMetadataReader.h
/// @brief Metadata reader built on the TagLib library.
/// @details Implements ITagReader to extract metadata from a wide range of
/// audio file formats using TagLib, including ID3v2 (with frames, ratings and
/// replay gain), MP4 (AAC/M4A) and generic Vorbis/FLAC/APE tags. Handles
/// track/disc totals, embedded artwork references and audio properties.

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

#include <musikcore/sdk/ITagReader.h>

/** @brief Reads audio metadata from files using TagLib.
 *  @details Dispatches to format-specific readers: a full ID3v2 reader for
 *  MP3/ID3v2 tags and a generic reader for all other supported formats. Values
 *  are written into an ITagStore for the library indexer. */
class TaglibMetadataReader : public musik::core::sdk::ITagReader {
    public:
        /** @brief Constructs a metadata reader. */
        TaglibMetadataReader();
        /** @brief Destroys the reader. */
        ~TaglibMetadataReader();

        /** @brief Reads metadata from a file into a tag store.
         *  @param uri The file URI to read.
         *  @param target The tag store to populate.
         *  @return True if metadata was read. */
        virtual bool Read(const char *uri, musik::core::sdk::ITagStore *target);
        /** @brief Returns whether this reader can read the given extension.
         *  @param extension The file extension.
         *  @return True if the format is supported. */
        virtual bool CanRead(const char *extension);
        /** @brief Destroys the reader. */
        virtual void Release();

    private:
        /** @brief Copies every entry of a TagLib map into the tag store.
         *  @tparam T Map type.
         *  @param map The TagLib property map.
         *  @param target The tag store to populate. */
        template <typename T> void ReadFromMap(
            const T& map,
            musik::core::sdk::ITagStore *target);

        /** @brief Copies a single map value into the tag store.
         *  @tparam T Map type.
         *  @param map The TagLib property map.
         *  @param inputKey Source key.
         *  @param outputKey Target tag key.
         *  @param target The tag store to populate. */
        template <typename T> void ExtractValueForKey(
            const T& map,
            const std::string& inputKey,
            const std::string& outputKey,
            musik::core::sdk::ITagStore *target);

        /** @brief Reads a single map value as a string.
         *  @tparam T Map type.
         *  @param map The TagLib property map.
         *  @param inputKey Source key.
         *  @param defaultValue Value returned when absent.
         *  @return The extracted value. */
        template <typename T> std::string ExtractValueForKey(
            const T& map,
            const std::string& inputKey,
            const std::string& defaultValue);

        /** @brief Extracts replay-gain values from a TagLib map.
         *  @tparam T Map type.
         *  @param map The TagLib property map.
         *  @param target The tag store to populate. */
        template <typename T> void ExtractReplayGain(
            const T& map,
            musik::core::sdk::ITagStore *target);

        /** @brief Reads the basic tag fields (title, artist, album, ...).
         *  @tparam T Tag type.
         *  @param tag The TagLib tag.
         *  @param uri The file URI.
         *  @param target The tag store to populate. */
        template<typename T> void ReadBasicData(
            const T* tag,
            const char* uri,
            musik::core::sdk::ITagStore *target);

        /** @brief Copies a single MP4 item map value into the tag store.
         *  @param map The MP4 item map.
         *  @param inputKey Source key.
         *  @param outputKey Target tag key.
         *  @param target The tag store to populate. */
        void ExtractValueForKey(
            const TagLib::MP4::ItemMap& map,
            const std::string& inputKey,
            const std::string& outputKey,
            musik::core::sdk::ITagStore *target);

        /** @brief Reads a single MP4 item map value as a string.
         *  @param map The MP4 item map.
         *  @param inputKey Source key.
         *  @param defaultValue Value returned when absent.
         *  @return The extracted value. */
        std::string ExtractValueForKey(
            const TagLib::MP4::ItemMap& map,
            const std::string& inputKey,
            const std::string& defaultValue);

        /** @brief Extracts a rating from an ID3v2 POPM frame.
         *  @param frame The POPM frame list.
         *  @return The rating value (0-255). */
        int ExtractRatingFromPopularimeter(
            const TagLib::ID3v2::FrameList& frame);

        /** @brief Sets a tag value and its "total" counterpart when possible.
         *  @param value The value (e.g. "3/12").
         *  @param valueKey The value tag key.
         *  @param totalKey The total tag key.
         *  @param track The tag store to populate. */
        void SetTagValueWithPossibleTotal(
            const std::string& value,
            const std::string& valueKey,
            const std::string& totalKey,
            musik::core::sdk::ITagStore* track
        );

        /** @brief Sets a string tag value.
         *  @param key The tag key.
         *  @param string The string value.
         *  @param target The tag store to populate. */
        void SetTagValue(
            const char* key,
            const char* string,
            musik::core::sdk::ITagStore *target);

        /** @brief Sets a TagLib string tag value.
         *  @param key The tag key.
         *  @param tagString The TagLib string value.
         *  @param target The tag store to populate. */
        void SetTagValue(
            const char* key,
            const TagLib::String tagString,
            musik::core::sdk::ITagStore *target);

        /** @brief Sets an integer tag value.
         *  @param key The tag key.
         *  @param tagInt The integer value.
         *  @param target The tag store to populate. */
        void SetTagValue(
            const char* key,
            const int tagInt,
            musik::core::sdk::ITagStore *target);

        /** @brief Sets a multi-value tag from an ID3v2 frame list.
         *  @param key The tag key.
         *  @param frame The frame list.
         *  @param target The tag store to populate. */
        void SetTagValues(const char* key,
            const TagLib::ID3v2::FrameList &frame,
            musik::core::sdk::ITagStore *target);

        /** @brief Copies audio properties (length, bitrate, channels, ...).
         *  @param audioProperties The TagLib audio properties.
         *  @param target The tag store to populate. */
        void SetAudioProperties(
            TagLib::AudioProperties *audioProperties,
            musik::core::sdk::ITagStore *target);

        /** @brief Sets slash-separated values from an ID3v2 frame list.
         *  @param key The tag key.
         *  @param frame The frame list.
         *  @param target The tag store to populate. */
        void SetSlashSeparatedValues(
            const char* key,
            const TagLib::ID3v2::FrameList &frame,
            musik::core::sdk::ITagStore *target);

        /** @brief Sets slash-separated values from a TagLib string.
         *  @param key The tag key.
         *  @param tagString The value string.
         *  @param target The tag store to populate. */
        void SetSlashSeparatedValues(
            const char* key,
            TagLib::String tagString,
            musik::core::sdk::ITagStore *target);

        /** @brief Reads an ID3v2 tag from a file.
         *  @param uri The file URI.
         *  @param target The tag store to populate.
         *  @return True on success. */
        bool ReadID3V2(
            const char* uri,
            musik::core::sdk::ITagStore *target);

        /** @brief Reads an in-memory ID3v2 tag.
         *  @param tag The ID3v2 tag.
         *  @param target The tag store to populate.
         *  @return True on success. */
        bool ReadID3V2(
            TagLib::ID3v2::Tag *tag,
            musik::core::sdk::ITagStore *target);

        /** @brief Reads metadata using the generic TagLib file reader.
         *  @param uri The file URI.
         *  @param extension The file extension.
         *  @param target The tag store to populate.
         *  @return True on success. */
        bool ReadGeneric(
            const char* uri,
            const std::string& extension,
            musik::core::sdk::ITagStore *target);
};
