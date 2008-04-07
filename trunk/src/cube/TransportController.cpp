//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, musikCube team
//
// Sources and Binaries of: mC2, win32cpp
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

#include <pch.hpp>
#include <cube/TransportController.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    TransportController::TransportController(TransportView& transportView)
: transportView(transportView)
{
    this->transportView.Created.connect(
        this, &TransportController::OnViewCreated);
    
    this->transportView.Resized.connect(
        this, &TransportController::OnViewResized);
}

void        TransportController::OnViewCreated()
{
    this->transportView.playButton->Pressed.connect(
        this, &TransportController::OnPlayPressed);

    this->transportView.stopButton->Pressed.connect(
        this, &TransportController::OnStopPressed);

    this->transportView.volumeSlider->Repositioned.connect(
        this, &TransportController::OnVolumeSliderChange);
    this->transportView.volumeSlider->SetPosition(transport.Volume()*100);
}

void        TransportController::OnViewResized(Size size)
{
}

void        TransportController::OnPlayPressed()
{
    transport.Start(_T("d:\\musik\\test.mp3"));
}

void        TransportController::OnStopPressed()
{
    transport.Stop(0);
}

void        TransportController::OnVolumeSliderChange()
{
    transport.ChangeVolume(transportView.volumeSlider->Position()/100.0f);
}
