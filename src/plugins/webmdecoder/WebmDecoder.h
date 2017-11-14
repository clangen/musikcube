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

#pragma once

#include <queue>
#include <vector>
#include <core/sdk/constants.h>
#include <core/sdk/IDecoder.h>
#include <core/sdk/IDataStream.h>
#include <webm/webm_parser.h>
#include <stddef.h>
#include "InternalDecoder.h"

/**
 * Decoder for webm files
 *
 * Webm is a container format, based on Matroska
 *
 * Structure:
 * EBML								Format information
 * Segment							Contains all other level 1 elements. Usually only 1
 * ├── SeekHead						Contains position info for level 1 elements
 * │   ├── Seek						Contains a single seek entry to an element
 * |   |   ├── SeekId				The binary ID corresponding to the element name
 * |   |   └── SeekPosition			The position of the element in the segment in octets (0 = first level 1 element)
 * │   ├── Seek
 * │   ├── Seek
 * │   └── Seek
 * ├── Void							Used to void damaged data, to avoid unexpected behaviors when using damaged data. The content is discarded. Also used to reserve space in a sub-element for later use
 * ├── Info							Contains miscellaneous general information and statistics on the file
 * ├── TimecodeScale				Timecode scale in nanoseconds (1.000.000 means all timecodes in the segment are expressed in milliseconds)
 * ├── Duration						Duration of the segment (based on TimecodeScale)
 * ├── Title						General name of the segment
 * ├── MuxingApp					Muxing application or library
 * ├── WritingApp					Writing application
 * ├── Tracks						A top-level block of information with many tracks described
 * │   └── TrackEntry				Describes a track with all elements
 * |       ├── TrackNumber			The track number as used in the Block Header (using more than 127 tracks is not encouraged, though the design allows an unlimited number)
 * |       ├── TrackUID				A unique ID to identify the Track. This should be kept the same when making a direct stream copy of the Track to another file
 * |       ├── TrackType			A set of track types coded on 8 bits (1: video, 2: audio, 3: complex, 0x10: logo, 0x11: subtitle, 0x12: buttons, 0x20: control)
 * |       ├── FlagEnabled			Set if the track is used
 * |       ├── FlagDefault			Set if that track (audio, video or subs) SHOULD be used if no language found matches the user preference
 * |       ├── FlagForced			Set if that track MUST be used during playback. There can be many forced track for a kind (audio, video or subs), the player should select the one which language matches the user preference or the default + forced track. Overlay MAY happen between a forced and non-forced track of the same kind
 * |       ├── FlagLacing			Set if the track may contain blocks using lacing
 * |       ├── DefaultDuration		Number of nanoseconds (i.e. not scaled) per frame
 * |       ├── Name					A human-readable track name
 * |       ├── Language				Specifies the language of the track in the Matroska languages form
 * |       ├── CodecID				An ID corresponding to the codec
 * |       ├── CodecPrivate			Private data only known to the codec
 * |       ├── CodecName			A human-readable string specifying the codec
 * |       ├── CodecDelay			CodecDelay is the codec-built-in delay in nanoseconds. This value must be subtracted from each block timestamp in order to get the actual timestamp. The value should be small so the muxing of tracks with the same actual timestamp are in the same Cluster
 * |       ├── SeekPreRoll			After a discontinuity, SeekPreRoll is the duration in nanoseconds of the data the decoder must decode before the decoded data is valid
 * │       ├── Video				Video settings
 * │       │   └── ...
 * │       ├── Audio				Audio settings
 * │       │   └── ...
 * |       └── ContentEncodings		Settings for several content encoding mechanisms like compression or encryption
 * │           └── ...
 * ├── Cues							A top-level element to speed seeking access. All entries are local to the segment
 * │   ├── CuePoint					Contains all information relative to a seek point in the segment
 * │   │   ├── CueTime				Absolute timecode according to the segment time base
 * │   │   ├── CueTrackPositions	Contain positions for different tracks corresponding to the timecode
 * │   │   ├── CueTrack				The track for which a position is given
 * │   │   └── CueClusterPosition	The position of the cluster containing the required block
 * │   ├── CuePoint
 * │   │   └── CueTrackPositions
 * │   ├── CuePoint
 * │   │   └── CueTrackPositions
 * ...
 * ├── Cluster						The lower level element containing the (monolithic) Block structure
 * │   ├── Timecode					Absolute timecode of the cluster (based on TimecodeScale)
 * │   ├── PrevSize					Size of the previous Cluster, in octets. Can be useful for backward playing
 * │   ├── SimpleBlock				Similar to Block but without all the extra information, mostly used to reduced overhead when no extra feature is needed
 * │   ├── SimpleBlock
 * │   ├── SimpleBlock
 * │   └── SimpleBlock
 * ├── Cluster
 * │   ├── SimpleBlock
 * │   ├── SimpleBlock
 * │   ├── SimpleBlock
 * │   ├── SimpleBlock
 * │   └── BlockGroup				Basic container of information containing a single Block or BlockVirtual, and information specific to that Block/VirtualBlock
 * │       └── Block				Block containing the actual data to be rendered and a timecode relative to the Cluster Timecode
 */
class WebmDecoder: public musik::core::sdk::IDecoder, public webm::Callback
{
public:
	WebmDecoder();
	virtual ~WebmDecoder();

	virtual void Release() override;
	virtual double SetPosition(double seconds) override;
	virtual bool GetBuffer(musik::core::sdk::IBuffer *buffer) override;
	virtual double GetDuration() override;
	virtual bool Open(musik::core::sdk::IDataStream *stream) override;
	virtual bool Exhausted() override
	{
		return this->exhausted;
	}

	////////////////////////////////////
	// webm parser callback functions //
	////////////////////////////////////
	webm::Status OnElementBegin(const webm::ElementMetadata& metadata,
			webm::Action* action) override;
	webm::Status OnUnknownElement(const webm::ElementMetadata& metadata,
			webm::Reader* reader, std::uint64_t* bytes_remaining) override;
	webm::Status OnEbml(const webm::ElementMetadata& metadata,
			const webm::Ebml& ebml) override;
	webm::Status OnVoid(const webm::ElementMetadata& metadata,
			webm::Reader* reader, std::uint64_t* bytes_remaining) override;
	webm::Status OnSegmentBegin(const webm::ElementMetadata& metadata,
			webm::Action* action) override;
	webm::Status OnSeek(const webm::ElementMetadata& metadata,
			const webm::Seek& seek) override;
	webm::Status OnInfo(const webm::ElementMetadata& metadata,
			const webm::Info& info) override;
	webm::Status OnClusterBegin(const webm::ElementMetadata& metadata,
			const webm::Cluster& cluster, webm::Action* action) override;
	webm::Status OnSimpleBlockBegin(const webm::ElementMetadata& metadata,
			const webm::SimpleBlock& simple_block, webm::Action* action)
					override;
	webm::Status OnSimpleBlockEnd(const webm::ElementMetadata& metadata,
			const webm::SimpleBlock& simple_block) override;
	webm::Status OnBlockGroupBegin(const webm::ElementMetadata& metadata,
			webm::Action* action) override;
	webm::Status OnBlockBegin(const webm::ElementMetadata& metadata,
			const webm::Block& block, webm::Action* action) override;
	webm::Status OnBlockEnd(const webm::ElementMetadata& metadata,
			const webm::Block& block) override;
	webm::Status OnBlockGroupEnd(const webm::ElementMetadata& metadata,
			const webm::BlockGroup& block_group) override;
	webm::Status OnFrame(const webm::FrameMetadata& metadata,
			webm::Reader* reader, std::uint64_t* bytes_remaining) override;
	webm::Status OnClusterEnd(const webm::ElementMetadata& metadata,
			const webm::Cluster& cluster) override;
	webm::Status OnTrackEntry(const webm::ElementMetadata& metadata,
			const webm::TrackEntry& track_entry) override;
	webm::Status OnCuePoint(const webm::ElementMetadata& metadata,
			const webm::CuePoint& cue_point) override;
	webm::Status OnEditionEntry(const webm::ElementMetadata& metadata,
			const webm::EditionEntry& edition_entry) override;
	webm::Status OnTag(const webm::ElementMetadata& metadata,
			const webm::Tag& tag) override;
	webm::Status OnSegmentEnd(const webm::ElementMetadata& metadata) override;

private:
	const std::string OPUS_ID = std::string("A_OPUS");
	const std::string VORBIS_ID = std::string("A_VORBIS");

	double duration;
	long channels;
	int bitsPerSample;
	long sampleRate;
	bool exhausted;

	std::queue<std::vector<std::uint8_t>> packets;
	std::unique_ptr<InternalDecoder> decoder;

	webm::WebmParser parser;
	std::unique_ptr<webm::Reader> reader;

	// Debugging functions
	void DumpMetadata(const webm::ElementMetadata& metadata);
	void DumpMetadata(const webm::FrameMetadata& metadata);
};

