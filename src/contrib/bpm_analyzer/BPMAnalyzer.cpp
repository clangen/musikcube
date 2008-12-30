//////////////////////////////////////////////////////////////////////////////
// Copyright  2007, Daniel Önnerby
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
#include "pch.h"
#include "BPMAnalyzer.h"
#include <boost/lexical_cast.hpp>
#include <core/config_format.h>

BPMAnalyzer::BPMAnalyzer()
 :detector(NULL)
{

}

BPMAnalyzer::~BPMAnalyzer(){
    delete this->detector;
}


void BPMAnalyzer::Destroy(){
    delete this;
}

bool BPMAnalyzer::Start(musik::core::ITrack *track){
    const utfchar *bpmchar  = track->GetValue("bpm");
    if(bpmchar){
        utfstring bpmstring(bpmchar);
        if(!bpmstring.empty()){
            try{
                double bpm  = boost::lexical_cast<double>(bpmstring);
                if(bpm>0){
                    return false;
                }
            }
            catch(...){
            }
        }
    }
    return true;
}

bool BPMAnalyzer::Analyze(musik::core::ITrack *track,IBuffer *buffer){
    if(buffer->Channels()>2){
        return false;
    }

    if(!this->detector){
        this->detector = new BPMDetect(buffer->Channels(),buffer->SampleRate());
    }

    this->detector->inputSamples(buffer->BufferPointer(),buffer->Samples());
    return true;
}

bool BPMAnalyzer::End(musik::core::ITrack *track){
    if(this->detector){
        float bpm   = this->detector->getBpm();
        if(bpm>0){
            try{
                utfstring bpmString   = boost::str( boost::utfformat(UTF("%.2f"))%bpm);
                track->SetValue("bpm",bpmString.c_str());
                return true;
            }
            catch(...){
            }
        }
    }
    return false;
}

