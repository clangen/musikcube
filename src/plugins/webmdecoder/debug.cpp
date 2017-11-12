//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <iostream>
#include <sstream>
#include "WebmDecoder.h"

void WebmDecoder::DumpMetadata(const webm::ElementMetadata& metadata) {
    std::stringstream ss;
    ss << "\tId: ";
    switch(static_cast<std::uint32_t>(metadata.id)) {
        case static_cast<std::uint32_t>(webm::Id::kEbml):
            ss << "EBML";
            break;
        case static_cast<std::uint32_t>(webm::Id::kEbmlVersion):
            ss << "EBMLVersion";
            break;
        case static_cast<std::uint32_t>(webm::Id::kEbmlReadVersion):
            ss << "EBMLReadVersion";
            break;
        case static_cast<std::uint32_t>(webm::Id::kEbmlMaxIdLength):
            ss << "EBMLMaxIDLength";
            break;
        case static_cast<std::uint32_t>(webm::Id::kEbmlMaxSizeLength):
            ss << "EBMLMaxSizeLength";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDocType):
            ss << "DocType";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDocTypeVersion):
            ss << "DocTypeVersion";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDocTypeReadVersion):
            ss << "DocTypeReadVersion";
            break;
        case static_cast<std::uint32_t>(webm::Id::kVoid):
            ss << "Void";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSegment):
            ss << "Segment";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSeekHead):
            ss << "SeekHead";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSeek):
            ss << "Seek";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSeekId):
            ss << "SeekId";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSeekPosition):
            ss << "SeekPosition";
            break;
        case static_cast<std::uint32_t>(webm::Id::kInfo):
            ss << "Info";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTimecodeScale):
            ss << "TimecodeScale";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDuration):
            ss << "Duration";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDateUtc):
            ss << "DateUTC";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTitle):
            ss << "Title";
            break;
        case static_cast<std::uint32_t>(webm::Id::kMuxingApp):
            ss << "MuxingApp";
            break;
        case static_cast<std::uint32_t>(webm::Id::kWritingApp):
            ss << "WritingApp";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCluster):
            ss << "Cluster";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTimecode):
            ss << "Timecode";
            break;
        case static_cast<std::uint32_t>(webm::Id::kPrevSize):
            ss << "PrevSize";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSimpleBlock):
            ss << "SimpleBlock";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockGroup):
            ss << "BlockGroup";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlock):
            ss << "Block";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockVirtual):
            ss << "BlockVirtual";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockAdditions):
            ss << "BlockAdditions";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockMore):
            ss << "BlockMore";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockAddId):
            ss << "BlockAddID";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockAdditional):
            ss << "BlockAdditional";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBlockDuration):
            ss << "BlockDuration";
            break;
        case static_cast<std::uint32_t>(webm::Id::kReferenceBlock):
            ss << "ReferenceBlock";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDiscardPadding):
            ss << "DiscardPadding";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSlices):
            ss << "Slices";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTimeSlice):
            ss << "TimeSlice";
            break;
        case static_cast<std::uint32_t>(webm::Id::kLaceNumber):
            ss << "LaceNumber";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTracks):
            ss << "Tracks";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTrackEntry):
            ss << "TrackEntry";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTrackNumber):
            ss << "TrackNumber";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTrackUid):
            ss << "TrackUID";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTrackType):
            ss << "TrackType";
            break;
        case static_cast<std::uint32_t>(webm::Id::kFlagEnabled):
            ss << "FlagEnabled";
            break;
        case static_cast<std::uint32_t>(webm::Id::kFlagDefault):
            ss << "FlagDefault";
            break;
        case static_cast<std::uint32_t>(webm::Id::kFlagForced):
            ss << "FlagForced";
            break;
        case static_cast<std::uint32_t>(webm::Id::kFlagLacing):
            ss << "FlagLacing";
            break;
        case static_cast<std::uint32_t>(webm::Id::kDefaultDuration):
            ss << "DefaultDuration";
            break;
        case static_cast<std::uint32_t>(webm::Id::kName):
            ss << "Name";
            break;
        case static_cast<std::uint32_t>(webm::Id::kLanguage):
            ss << "Language";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCodecId):
            ss << "CodecID";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCodecPrivate):
            ss << "CodecPrivate";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCodecName):
            ss << "CodecName";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCodecDelay):
            ss << "CodecDelay";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSeekPreRoll):
            ss << "SeekPreRoll";
            break;
        case static_cast<std::uint32_t>(webm::Id::kStereoMode):
            ss << "StereoMode";
            break;
        case static_cast<std::uint32_t>(webm::Id::kFrameRate):
            ss << "FrameRate";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBitsPerChannel):
            ss << "BitsPerChannel";
            break;
        case static_cast<std::uint32_t>(webm::Id::kRange):
            ss << "Range";
            break;
        case static_cast<std::uint32_t>(webm::Id::kAudio):
            ss << "Audio";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSamplingFrequency):
            ss << "SamplingFrequency";
            break;
        case static_cast<std::uint32_t>(webm::Id::kOutputSamplingFrequency):
            ss << "OutputSamplingFrequency";
            break;
        case static_cast<std::uint32_t>(webm::Id::kChannels):
            ss << "Channels";
            break;
        case static_cast<std::uint32_t>(webm::Id::kBitDepth):
            ss << "BitDepth";
            break;
        case static_cast<std::uint32_t>(webm::Id::kContentEncodings):
            ss << "ContentEncodings";
            break;
        case static_cast<std::uint32_t>(webm::Id::kContentEncoding):
            ss << "ContentEncoding";
            break;
        case static_cast<std::uint32_t>(webm::Id::kContentEncodingOrder):
            ss << "ContentEncodingOrder";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCues):
            ss << "Cues";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCuePoint):
            ss << "CuePoint";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCueTime):
            ss << "CueTime";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCueTrackPositions):
            ss << "CueTrackPositions";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCueTrack):
            ss << "CueTrack";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCueClusterPosition):
            ss << "CueClusterPosition";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCueRelativePosition):
            ss << "CueRelativePosition";
            break;
        case static_cast<std::uint32_t>(webm::Id::kCueDuration):
            ss << "CueDuration";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTags):
            ss << "Tags";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTag):
            ss << "Tag";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTargets):
            ss << "Targets";
            break;
        case static_cast<std::uint32_t>(webm::Id::kSimpleTag):
            ss << "SimpleTag";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTagName):
            ss << "TagName";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTagLanguage):
            ss << "TagLanguage";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTagDefault):
            ss << "TagDefault";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTagString):
            ss << "TagString";
            break;
        case static_cast<std::uint32_t>(webm::Id::kTagBinary):
            ss << "TagBinary";
            break;
        default:
            ss << "Unrecognised - " << (int) metadata.id;
    }
    std::cerr << ss.str() << std::endl;
    std::cerr << "\tHeader size: " << metadata.header_size << std::endl;
    std::cerr << "\tSize: " << metadata.size << std::endl;
    std::cerr << "\tPosition: " << metadata.position << std::endl;
}

void WebmDecoder::DumpMetadata(const webm::FrameMetadata& metadata)
{
    std::cerr << "\tParent position: " << metadata.parent_element.position << std::endl;
    std::cerr << "\tSize: " << metadata.size << std::endl;
    std::cerr << "\tPosition: " << metadata.position << std::endl;
}



