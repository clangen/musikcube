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
#include "CallbackHandler.h"

CallbackHandler::CallbackHandler()
: duration(-1.0f)
{};

void CallbackHandler::DumpMetadata(const webm::ElementMetadata& metadata) {
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

void CallbackHandler::DumpMetadata(const webm::FrameMetadata& metadata)
{
    std::cerr << "\tParent position: " << metadata.parent_element.position << std::endl;
    std::cerr << "\tSize: " << metadata.size << std::endl;
    std::cerr << "\tPosition: " << metadata.position << std::endl;
}

webm::Status CallbackHandler::OnElementBegin(const webm::ElementMetadata& metadata,
                                             webm::Action* action)
{
    std::cerr << "OnElementBegin" << std::endl;
    DumpMetadata(metadata);
    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnUnknownElement(const webm::ElementMetadata& metadata,
                                               webm::Reader* reader,
                                               std::uint64_t* bytes_remaining)
{
    std::cerr << "OnUnknownElement" << std::endl;
    DumpMetadata(metadata);
    std::uint64_t bytes_to_read = *bytes_remaining;
    return reader->Skip(bytes_to_read, bytes_remaining);
}
webm::Status CallbackHandler::OnEbml(const webm::ElementMetadata& metadata,
                                     const webm::Ebml& ebml)
{
    std::cerr << "OnEbml" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnVoid(const webm::ElementMetadata& metadata,
                                     webm::Reader* reader,
                                     std::uint64_t* bytes_remaining)
{
    std::cerr << "OnVoid" << std::endl;
    DumpMetadata(metadata);
    std::uint64_t bytes_skipped;
    webm::Status status = reader->Skip(*bytes_remaining, &bytes_skipped);
    *bytes_remaining -= bytes_skipped;
    return status;
}
webm::Status CallbackHandler::OnSegmentBegin(const webm::ElementMetadata& metadata,
                                             webm::Action* action)
{
    std::cerr << "OnSegmentBegin" << std::endl;
    DumpMetadata(metadata);
    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnSeek(const webm::ElementMetadata& metadata,
                                     const webm::Seek& seek)
{
    std::cerr << "OnSeek" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnInfo(const webm::ElementMetadata& metadata,
                                     const webm::Info& info)
{
    std::cerr << "OnInfo" << std::endl;
    DumpMetadata(metadata);
    std::cerr << "\tTimecode scale - " << info.timecode_scale.value() << std::endl;
    std::cerr << "\tDuration - " << info.duration.value() << std::endl;
    this->duration = static_cast<double>(info.duration.value());
    std::cerr << "\tDate - " << info.date_utc.value() << std::endl;
    std::cerr << "\tTitle - " << info.title.value() << std::endl;
    std::cerr << "\tMuxing app - " << info.muxing_app.value() << std::endl;
    std::cerr << "\tWriting app - " << info.writing_app.value() << std::endl;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnClusterBegin(const webm::ElementMetadata& metadata,
                                             const webm::Cluster& cluster,
                                             webm::Action* action)
{
    std::cerr << "OnClusterBegin" << std::endl;
    DumpMetadata(metadata);
    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnSimpleBlockBegin(const webm::ElementMetadata& metadata,
                                                 const webm::SimpleBlock& simple_block,
                                                 webm::Action* action)
{
    std::cerr << "OnSimpleBlockBegin" << std::endl;
    DumpMetadata(metadata);
    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnSimpleBlockEnd(const webm::ElementMetadata& metadata,
                                               const webm::SimpleBlock& simple_block)
{
    std::cerr << "OnSimpleBlockEnd" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnBlockGroupBegin(const webm::ElementMetadata& metadata,
                                                webm::Action* action)
{
    std::cerr << "OnBlockGroupBegin" << std::endl;
    DumpMetadata(metadata);
    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnBlockBegin(const webm::ElementMetadata& metadata,
                                           const webm::Block& block,
                                           webm::Action* action)
{
    std::cerr << "OnBlockBegin" << std::endl;
    DumpMetadata(metadata);
    *action = webm::Action::kRead;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnBlockEnd(const webm::ElementMetadata& metadata,
                                         const webm::Block& block)
{
    std::cerr << "OnBlockEnd" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnBlockGroupEnd(const webm::ElementMetadata& metadata,
                                              const webm::BlockGroup& block_group)
{
    std::cerr << "OnBlockGroupEnd" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnFrame(const webm::FrameMetadata& metadata,
                                      webm::Reader* reader,
                                      std::uint64_t* bytes_remaining)
{
    std::cerr << "OnFrame" << std::endl;
    DumpMetadata(metadata);
    std::uint64_t bytes_skipped;
    std::cerr << "Bytes to read: " << *bytes_remaining << std::endl;
    webm::Status status = reader->Skip(*bytes_remaining, &bytes_skipped);
    *bytes_remaining -= bytes_skipped;
    return status;
}
webm::Status CallbackHandler::OnClusterEnd(const webm::ElementMetadata& metadata,
                                           const webm::Cluster& cluster)
{
    std::cerr << "OnClusterEnd" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnTrackEntry(const webm::ElementMetadata& metadata,
                                           const webm::TrackEntry& track_entry)
{
    std::cerr << "OnTrackEntry" << std::endl;
    DumpMetadata(metadata);
    std::cerr << "\tTrack number - " << track_entry.track_number.value() << std::endl;
    std::cerr << "\tTrack UID" << track_entry.track_uid.value() << std::endl;
    std::stringstream ss;
    ss << "\tTrack type - ";
    auto track_type = track_entry.track_type.value();
    switch (static_cast<std::uint64_t>(track_type)) {
        case static_cast<std::uint64_t>(webm::TrackType::kVideo):
            ss << "Video";
            break;
        case static_cast<std::uint64_t>(webm::TrackType::kAudio):
            ss << "Audio";
            break;
        case static_cast<std::uint64_t>(webm::TrackType::kComplex):
            ss << "Complex";
            break;
        case static_cast<std::uint64_t>(webm::TrackType::kLogo):
            ss << "Logo";
            break;
        case static_cast<std::uint64_t>(webm::TrackType::kSubtitle):
            ss << "Subtitle";
            break;
        case static_cast<std::uint64_t>(webm::TrackType::kButtons):
            ss << "Buttons";
            break;
        case static_cast<std::uint64_t>(webm::TrackType::kControl):
            ss << "Control";
            break;
    }
    std::cerr << ss.str() << std::endl;
    std::cerr << "\tIs enabled - " << track_entry.is_enabled.value() << std::endl;
    std::cerr << "\tIs default - " << track_entry.is_default.value() << std::endl;
    std::cerr << "\tIs forced - " << track_entry.is_forced.value() << std::endl;
    std::cerr << "\tUses lacing - " << track_entry.uses_lacing.value() << std::endl;
    std::cerr << "\tDefault duration - " << track_entry.default_duration.value() << std::endl;
    std::cerr << "\tName - " << track_entry.name.value() << std::endl;
    std::cerr << "\tLanguage - " << track_entry.language.value() << std::endl;
    std::cerr << "\tCodec ID - " << track_entry.codec_id.value() << std::endl;
    std::cerr << "\tCodec name - " << track_entry.codec_name.value() << std::endl;
    std::cerr << "\tCodec delay - " << track_entry.codec_delay.value() << std::endl;
    std::cerr << "\tSeek pre roll - " << track_entry.seek_pre_roll.value() << std::endl;
    std::cerr << "\tVideo (ignored)" << std::endl;
    //auto video = track_entry.video.value();
    std::cerr << "\tAudio" << std::endl;
    auto audio = track_entry.audio.value();
    std::cerr << "\t\tSample rate - " << audio.sampling_frequency.value() << std::endl;
    sample_rate = static_cast<long>(audio.sampling_frequency.value());
    std::cerr << "\t\tOutput freq - " << audio.output_frequency.value() << std::endl;
    std::cerr << "\t\tChannels - " << audio.channels.value() << std::endl;
    channels = static_cast<int>(audio.channels.value());
    std::cerr << "\t\tBit depth - " << audio.bit_depth.value() << std::endl;
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnCuePoint(const webm::ElementMetadata& metadata,
                                         const webm::CuePoint& cue_point)
{
    std::cerr << "OnCuePoint" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnEditionEntry(const webm::ElementMetadata& metadata,
                                             const webm::EditionEntry& edition_entry)
{
    std::cerr << "OnEditionEntry" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnTag(const webm::ElementMetadata& metadata,
                                    const webm::Tag& tag)
{
    std::cerr << "OnTag" << std::endl;
    DumpMetadata(metadata);
    for (auto i = tag.tags.begin(); i <= tag.tags.end(); i++) {
        auto tag = (*i).value();
        std::cerr << "\tName - " << tag.name.value() << std::endl;
        std::cerr << "\tLanguage - " << tag.language.value() << std::endl;
        std::cerr << "\tIs default - " << tag.is_default.value() << std::endl;
        std::cerr << "\tString - " << tag.string.value() << std::endl;
        //std::cerr << "\tBinary - " << tag.binary.value() << std::endl;
    }
    return webm::Status(webm::Status::kOkCompleted);
}
webm::Status CallbackHandler::OnSegmentEnd(const webm::ElementMetadata& metadata)
{
    std::cerr << "OnSegmentEnd" << std::endl;
    DumpMetadata(metadata);
    return webm::Status(webm::Status::kOkCompleted);
}

