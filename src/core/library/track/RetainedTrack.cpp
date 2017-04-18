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

#include "pch.hpp"
#include "RetainedTrack.h"

using namespace musik::core;

/* * * * RetainedTrack * * * */

RetainedTrack::RetainedTrack(TrackPtr track) {
    this->count = 1;
    this->track = track;
}

RetainedTrack::~RetainedTrack() {
}

void RetainedTrack::Release() {
    int c = this->count.fetch_sub(1);
    if (c > 0) {
        this->count = 0;
        this->track.reset();
        delete this;
    }
}

void RetainedTrack::Retain() {
    ++this->count;
}

int RetainedTrack::GetValue(const char* key, char* dst, int size) {
    return track->GetValue(key, dst, size);
}

int RetainedTrack::Uri(char* dst, int size) {
    return track->Uri(dst, size);
}

uint64_t RetainedTrack::GetUint64(const char* key, uint64_t defaultValue) {
    return track->GetUint64(key, defaultValue);
}

long long RetainedTrack::GetInt64(const char* key, long long defaultValue) {
    return track->GetInt64(key, defaultValue);
}

unsigned int RetainedTrack::GetUint32(const char* key, unsigned long defaultValue) {
    return track->GetUint32(key, defaultValue);
}

int RetainedTrack::GetInt32(const char* key, unsigned int defaultValue) {
    return track->GetInt32(key, defaultValue);
}

double RetainedTrack::GetDouble(const char* key, double defaultValue) {
    return track->GetDouble(key, defaultValue);
}

uint64_t RetainedTrack::GetId() {
    return track->GetId();
}

/* * * * RetainedTrackWriter * * * */

RetainedTrackWriter::RetainedTrackWriter(TrackPtr track) {
    this->count = 1;
    this->track = track;
}

RetainedTrackWriter::~RetainedTrackWriter() {
}

void RetainedTrackWriter::Release() {
    int c = this->count.fetch_sub(1);
    if (c > 0) {
        this->count = 0;
        this->track.reset();
        delete this;
    }
}

void RetainedTrackWriter::Retain() {
    ++this->count;
}

void RetainedTrackWriter::SetValue(const char* metakey, const char* value) {
    this->track->SetValue(metakey, value);
}

void RetainedTrackWriter::ClearValue(const char* metakey) {
    this->track->ClearValue(metakey);
}

void RetainedTrackWriter::SetThumbnail(const char *data, long size) {
    this->track->SetThumbnail(data, size);
}