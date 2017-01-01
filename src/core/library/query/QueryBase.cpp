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

#include <core/library/query/QueryBase.h>
#include <core/library/LocalLibrary.h>
#include <atomic>

using namespace musik::core;
using namespace musik::core::query;

static std::atomic<int> nextId(0);

QueryBase::QueryBase()
: status(0)
, options(0)
, queryId(0)
, cancel(false) {
    this->queryId = nextId++;
}

QueryBase::~QueryBase() {
}

std::string QueryBase::Name() {
    return "QueryBase";
}

bool QueryBase::Run(db::Connection &db) {
    this->SetStatus(Running);
    try {
        if (this->IsCanceled()) {
            this->SetStatus(Canceled);
            return true;
        }
        else if (OnRun(db)) {
            this->SetStatus(Finished);
            return true;
        }
    }
    catch (...) {
    }

    this->SetStatus(Failed);
    return false;
}

int QueryBase::GetStatus() {
    std::unique_lock<std::mutex> lock(this->stateMutex);
    return this->status;
}

void QueryBase::SetStatus(int status) {
    std::unique_lock<std::mutex> lock(this->stateMutex);
    this->status = status;
}

int QueryBase::GetId() {
    std::unique_lock<std::mutex> lock(this->stateMutex);
    return this->queryId;
}

int QueryBase::GetOptions() {
    std::unique_lock<std::mutex> lock(this->stateMutex);
    return this->options;
}

void QueryBase::SetOptions(int options) {
    std::unique_lock<std::mutex> lock(this->stateMutex);
    this->options = options;
}
