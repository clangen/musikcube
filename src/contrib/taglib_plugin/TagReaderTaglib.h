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

#pragma once

#include "stdafx.h"

#ifndef _HAVE_TAGLIB
#include <toolkit/tlist.h>
#include <toolkit/tfile.h>

#include <taglib/tag.h>
#include <taglib/fileref.h>
#include <taglib/audioproperties.h>

#include <mpeg/id3v2/id3v2tag.h>
#else //_HAVE_TAGLIB
#include <toolkit/tlist.h>
#include <toolkit/tfile.h>

#include <tag.h>
#include <fileref.h>
#include <audioproperties.h>

#include <taglib/mpeg/id3v2/id3v2tag.h>
#endif //_HAVE_TAGLIB


#include <set>

#include <core/Plugin/IMetaDataReader.h>
#include <core/Common.h>

class TagReaderTaglib : public musik::core::Plugin::IMetaDataReader {
	public:
		TagReaderTaglib(void);
		virtual ~TagReaderTaglib(void);
		bool ReadTag(musik::core::ITrack *track);
		virtual bool CanReadTag(const utfchar *extension);
        virtual void Destroy();
	private:
//		bool getStandardTags(musik::core::ITrack &track,TagLib::Tag *oTag,TagLib::AudioProperties *oAudioProperties);

		void SetTagValue(const char* key,const utfchar* string,musik::core::ITrack *track);
		void SetTagValue(const char* key,const TagLib::String tagString,musik::core::ITrack *track);
		void SetTagValue(const char* key,const int tagInt,musik::core::ITrack *track);
		void SetTagValues(const char* key,const TagLib::ID3v2::FrameList &frame,musik::core::ITrack *track);
        void SetAudioProperties(TagLib::AudioProperties *audioProperties,musik::core::ITrack *track);

		void SetSlashSeparatedValues(const char* key,const TagLib::ID3v2::FrameList &frame,musik::core::ITrack *track);
        void SetSlashSeparatedValues(const char* key,TagLib::String tagString,musik::core::ITrack *track);

        bool GetID3v2Tag(musik::core::ITrack *track);
        bool GetGenericTag(musik::core::ITrack *track);
        bool GetOGGTag(musik::core::ITrack *track);
};

