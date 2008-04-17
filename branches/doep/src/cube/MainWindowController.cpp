//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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
#include <cube/MainWindowController.hpp>
#include <cube/TransportView.hpp>
#include <cube/TracklistView.hpp>
#include <cube/SourcesView.hpp>

#include <core/Library/LocalDB.h>

//////////////////////////////////////////////////////////////////////////////

using namespace musik::cube;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    MainWindowController::MainWindowController(TopLevelWindow& mainWindow)
: mainWindow(mainWindow)
, sourcesController(NULL)
, transportController(NULL)
, menuController(mainWindow)
, LibraryCallbackTimer(20)
{
    musik::core::PluginFactory::Instance();

    this->mainWindow.Created.connect(
        this, &MainWindowController::OnMainWindowCreated);

    // Connect the local library to the 
    this->LibraryCallbackTimer.ConnectToWindow(&mainWindow);
    this->LibraryCallbackTimer.OnTimout.connect(this,&MainWindowController::QueryQueueLoop);
    musik::core::LibraryFactory::GetCurrentLibrary()->OnQueryQueueStart.connect(this,&MainWindowController::QueryQueueStart);
    musik::core::LibraryFactory::GetCurrentLibrary()->OnQueryQueueEnd.connect(this,&MainWindowController::QueryQueueEnd);

}

MainWindowController::~MainWindowController()
{
    delete this->sourcesController;
    delete this->transportController;
}

void        MainWindowController::OnMainWindowCreated()
{
    static const int TransportViewHeight = 54;

    this->mainWindow.MoveTo(200, 200);
    this->mainWindow.Resize(700, 480);
    this->mainWindow.SetMinimumSize(Size(320, 240));

    Size clientSize = this->mainWindow.ClientSize();

    // create transport view/controller
    TransportView* transportView = new TransportView();
    transportView->SetPadding(FramePadding(2, 4, 0, 0));
    this->transportController = new TransportController(*transportView);

    // create sources view/controller
    SourcesView* sourcesView = new SourcesView();
    this->sourcesController = new SourcesController(*sourcesView);

    // the main splitter
    Splitter* transportSplitter = this->mainWindow.AddChild(
        new Splitter(SplitVertical, sourcesView, transportView));

    // set initial sizes
    transportSplitter->Resize(clientSize);
    transportSplitter->SetAnchor(AnchorBottom);
    transportSplitter->SetAnchorSize(TransportViewHeight);
    transportSplitter->SetSizable(false);

    this->clientView = transportSplitter;
    this->mainWindow.Resized.connect(this, &MainWindowController::OnResize);
}

void        MainWindowController::OnResize(Size size)
{
    RedrawLock redrawLock(this->clientView);
    this->clientView->Resize(this->mainWindow.ClientSize());
}

void MainWindowController::QueryQueueStart(){
    this->LibraryCallbackTimer.ConnectToWindow(&this->mainWindow);
    this->LibraryCallbackTimer.Start();
}

void MainWindowController::QueryQueueEnd(){
    this->LibraryCallbackTimer.Stop();
}

void MainWindowController::QueryQueueLoop(){
    musik::core::LibraryFactory::GetCurrentLibrary()->RunCallbacks();
}
