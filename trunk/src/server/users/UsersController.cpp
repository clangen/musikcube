//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, mC2 Team
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

#include "pch.hpp"
#include <server/users/UsersController.hpp>
#include <server/users/UsersView.hpp>
#include <server/users/UsersListController.hpp>
#include <server/users/EditUserController.hpp>

#include <win32cpp/Button.hpp>
#include <win32cpp/Label.hpp>
#include <win32cpp/Window.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;
using namespace musik::server::users;

//////////////////////////////////////////////////////////////////////////////

UsersController::UsersController(UsersView& usersView,musik::core::Server *server)
 :usersView(usersView)
 ,server(server)
{
    this->usersView.Handle()
        ? this->OnViewCreated(&this->usersView)
        : this->usersView.Created.connect(this, &UsersController::OnViewCreated);
    
    this->usersView.Resized.connect(
        this, &UsersController::OnViewResized);
}

void UsersController::OnViewCreated(Window* window)
{

    this->usersView.addUserButton->Pressed.connect(this,&UsersController::OnAddUser);
    this->usersView.removeUserButton->Pressed.connect(this,&UsersController::OnRemoveUser);

    this->usersListController.reset(new UsersListController(*this->usersView.usersList,this));

}

void        UsersController::OnViewResized(Window* window, Size size)
{
}

void UsersController::OnAddUser(Button* button){
    win32cpp::TopLevelWindow popupDialog(_T("Add user"));
    popupDialog.SetMinimumSize(Size(250, 250));

    EditUserController addUser(popupDialog,this->server);

    popupDialog.ShowModal(win32cpp::TopLevelWindow::FindFromAncestor(&this->usersView));
}

void UsersController::OnRemoveUser(Button* button){
    this->usersListController->RemoveSelectedUsers();
}


