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

#include "stdafx.h"
#include "WebmDecoder.h"
#include "IDataStreamReader.h"
#include "InternalOpusDecoder.h"
#include <core/debug.h>
#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>

WebmDecoder::WebmDecoder() :
		stream(nullptr), decoder(nullptr), bitsPerSample(0), totalSamples(0), channels(
				0), duration(0), exhausted(false)
{
}

WebmDecoder::~WebmDecoder()
{
}

void WebmDecoder::Release()
{
	if (this->decoder != nullptr)
		delete this->decoder;
	delete this;
}

double WebmDecoder::SetPosition(double seconds)
{
	return -1;
}

double WebmDecoder::GetDuration()
{
	return duration;
}

bool WebmDecoder::GetBuffer(musik::core::sdk::IBuffer *buffer)
{
	buffer->SetSampleRate(48000);
	buffer->SetSamples(totalSamples);
	buffer->SetChannels(channels);
	std::memcpy(buffer->BufferPointer(), this->decodedBuffer.data(), this->decodedBuffer.size() * sizeof(float));
	this->exhausted = true;
	return true;
}

bool WebmDecoder::Open(musik::core::sdk::IDataStream *stream)
{
	reader = std::unique_ptr<webm::Reader>(new IDataStreamReader(stream));
	webm::Status status = parser.Feed(this, reader.get());
	while (status.ok() && !status.completed_ok())
	{
		status = parser.Feed(this, reader.get());
	}
	if (!status.completed_ok())
	{
		std::cerr << "WebmDecoder: Open stream failed - " << status.code
				<< std::endl;
		this->exhausted = true;
		return false;
	}
	std::cerr << "Open complete" << std::endl;
	return true;
}

// webm parser callback functions

webm::Status WebmDecoder::OnElementBegin(const webm::ElementMetadata& metadata,
		webm::Action* action)
{
	*action = webm::Action::kRead;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnUnknownElement(
		const webm::ElementMetadata& metadata, webm::Reader* reader,
		std::uint64_t* bytes_remaining)
{
	std::cerr << "OnUnknownElement" << std::endl;
	DumpMetadata(metadata);
	std::uint64_t bytes_to_read = *bytes_remaining;
	return reader->Skip(bytes_to_read, bytes_remaining);
}
webm::Status WebmDecoder::OnEbml(const webm::ElementMetadata& metadata,
		const webm::Ebml& ebml)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnVoid(const webm::ElementMetadata& metadata,
		webm::Reader* reader, std::uint64_t* bytes_remaining)
{
	std::uint64_t bytes_skipped;
	webm::Status status = reader->Skip(*bytes_remaining, &bytes_skipped);
	*bytes_remaining -= bytes_skipped;
	return status;
}
webm::Status WebmDecoder::OnSegmentBegin(const webm::ElementMetadata& metadata,
		webm::Action* action)
{
	*action = webm::Action::kRead;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnSeek(const webm::ElementMetadata& metadata,
		const webm::Seek& seek)
{
	std::cerr << "OnSeek" << std::endl;
	DumpMetadata(metadata);
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnInfo(const webm::ElementMetadata& metadata,
		const webm::Info& info)
{
	std::cerr << "OnInfo" << std::endl;
	DumpMetadata(metadata);
	std::cerr << "\tTimecode scale - " << info.timecode_scale.value()
			<< std::endl;
	std::cerr << "\tDuration - " << info.duration.value() << std::endl;
	this->duration = static_cast<double>(info.duration.value() / 1000);
	std::cerr << "\tDate - " << info.date_utc.value() << std::endl;
	std::cerr << "\tTitle - " << info.title.value() << std::endl;
	std::cerr << "\tMuxing app - " << info.muxing_app.value() << std::endl;
	std::cerr << "\tWriting app - " << info.writing_app.value() << std::endl;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnClusterBegin(const webm::ElementMetadata& metadata,
		const webm::Cluster& cluster, webm::Action* action)
{
	*action = webm::Action::kRead;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnSimpleBlockBegin(
		const webm::ElementMetadata& metadata,
		const webm::SimpleBlock& simple_block, webm::Action* action)
{
	*action = webm::Action::kRead;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnSimpleBlockEnd(
		const webm::ElementMetadata& metadata,
		const webm::SimpleBlock& simple_block)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnBlockGroupBegin(
		const webm::ElementMetadata& metadata, webm::Action* action)
{
	*action = webm::Action::kRead;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnBlockBegin(const webm::ElementMetadata& metadata,
		const webm::Block& block, webm::Action* action)
{
	*action = webm::Action::kRead;
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnBlockEnd(const webm::ElementMetadata& metadata,
		const webm::Block& block)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnBlockGroupEnd(const webm::ElementMetadata& metadata,
		const webm::BlockGroup& block_group)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnFrame(const webm::FrameMetadata& metadata,
		webm::Reader* reader, std::uint64_t* bytes_remaining)
{
	std::uint64_t bytes_read;
	std::uint8_t buffer[*bytes_remaining];
	webm::Status status = reader->Read(metadata.size, buffer, &bytes_read);
	*bytes_remaining -= bytes_read;
	if (this->decoder != nullptr)
	{
		std::vector<std::uint8_t> data;
		data.insert(data.end(), &buffer[0], &buffer[0] + bytes_read);
		int samples = this->decoder->DecodeData(decodedBuffer, channels, data);
		if (samples > 0)
		{
			totalSamples += (samples * channels);
		} else {
			std::cerr << "webmdecoder: failed to decode frame - " << samples << std::endl;
		}
	}

	return status;
}
webm::Status WebmDecoder::OnClusterEnd(const webm::ElementMetadata& metadata,
		const webm::Cluster& cluster)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnTrackEntry(const webm::ElementMetadata& metadata,
		const webm::TrackEntry& track_entry)
{
	std::cerr << "OnTrackEntry" << std::endl;
	DumpMetadata(metadata);
	std::cerr << "\tTrack number - " << track_entry.track_number.value()
			<< std::endl;
	std::cerr << "\tTrack UID" << track_entry.track_uid.value() << std::endl;
	std::stringstream ss;
	ss << "\tTrack type - ";
	auto track_type = track_entry.track_type.value();
	switch (static_cast<std::uint64_t>(track_type))
	{
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
	std::cerr << "\tIs enabled - " << track_entry.is_enabled.value()
			<< std::endl;
	std::cerr << "\tIs default - " << track_entry.is_default.value()
			<< std::endl;
	std::cerr << "\tIs forced - " << track_entry.is_forced.value() << std::endl;
	std::cerr << "\tUses lacing - " << track_entry.uses_lacing.value()
			<< std::endl;
	std::cerr << "\tDefault duration - " << track_entry.default_duration.value()
			<< std::endl;
	std::cerr << "\tName - " << track_entry.name.value() << std::endl;
	std::cerr << "\tLanguage - " << track_entry.language.value() << std::endl;
	std::string codecId = track_entry.codec_id.value();
	std::cerr << "\tCodec ID - " << codecId << std::endl;
	std::cerr << "\tCodec name - " << track_entry.codec_name.value()
			<< std::endl;
	std::cerr << "\tCodec delay - " << track_entry.codec_delay.value()
			<< std::endl;
	std::cerr << "\tSeek pre roll - " << track_entry.seek_pre_roll.value()
			<< std::endl;
	std::cerr << "\tVideo (ignored)" << std::endl;
	//auto video = track_entry.video.value();
	std::cerr << "\tAudio" << std::endl;
	auto audio = track_entry.audio.value();
	std::cerr << "\t\tSample rate - " << audio.sampling_frequency.value()
			<< std::endl;
	std::cerr << "\t\tOutput freq - " << audio.output_frequency.value()
			<< std::endl;
	std::cerr << "\t\tChannels - " << audio.channels.value() << std::endl;
	channels = static_cast<int>(audio.channels.value());
	std::cerr << "\t\tBit depth - " << audio.bit_depth.value() << std::endl;
	try
	{
		if (codecId == OPUS_ID)
		{
			this->decoder = new InternalOpusDecoder(audio.channels.value());
		}
		else if (codecId == VORBIS_ID)
		{
			throw std::runtime_error("Vorbis encoded audio not yet supported");
		}
	} catch (std::runtime_error& e)
	{
		std::cerr << "webmdecoder: Unable to create decoder: " << e.what()
				<< std::endl;
	}
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnCuePoint(const webm::ElementMetadata& metadata,
		const webm::CuePoint& cue_point)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnEditionEntry(const webm::ElementMetadata& metadata,
		const webm::EditionEntry& edition_entry)
{
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnTag(const webm::ElementMetadata& metadata,
		const webm::Tag& tag)
{
	std::cerr << "OnTag" << std::endl;
	DumpMetadata(metadata);
	for (auto i = tag.tags.begin(); i <= tag.tags.end(); i++)
	{
		auto tag = (*i).value();
		std::cerr << "\tName - " << tag.name.value() << std::endl;
		std::cerr << "\tLanguage - " << tag.language.value() << std::endl;
		std::cerr << "\tIs default - " << tag.is_default.value() << std::endl;
		std::cerr << "\tString - " << tag.string.value() << std::endl;
		//std::cerr << "\tBinary - " << tag.binary.value() << std::endl;
	}
	return webm::Status(webm::Status::kOkCompleted);
}
webm::Status WebmDecoder::OnSegmentEnd(const webm::ElementMetadata& metadata)
{
	return webm::Status(webm::Status::kOkCompleted);
}
