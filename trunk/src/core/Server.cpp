//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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
#include "Server.h"

#include <core/Preferences.h>
#include <core/Common.h>
#include <core/Library/Base.h>
#include <core/Crypt.h>

#include <boost/bind.hpp>


using namespace musik::core;


Server::Server(unsigned int port,unsigned int httpPort)
 :exitThread(false)
 ,acceptor(ioService,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
 ,httpServer(httpPort)
{
    this->acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
}

Server::~Server(void){
    this->connections.clear();
    this->nextConnection.reset();

    this->Exit();

    this->threads.join_all();
}

void Server::Exit(){
    boost::mutex::scoped_lock lock(this->serverMutex);
    this->exitThread    = true;
    this->ioService.stop();
}

bool Server::Exited(){
    boost::mutex::scoped_lock lock(this->serverMutex);
    return this->exitThread;
}

bool Server::Startup(){
    // Start the server thread
    this->threads.create_thread(boost::bind(&Server::ThreadLoop,this));

    return true;
}

utfstring Server::ServerIdentifier(){
    return UTF("server");
}


void Server::ThreadLoop(){
    // Get server preferences
    musik::core::Preferences prefs("Server");

    // Get directory and database paths
    utfstring directory( musik::core::GetDataDirectory()+this->ServerIdentifier()+UTF("/") );
    utfstring database(directory+UTF("musik.db"));

    // Create directory if not existing
    boost::filesystem::utfpath folder(directory);
    if(!boost::filesystem::exists(folder)){
        boost::filesystem::create_directories(folder);
    }

    // Create database
    this->db.Open(database.c_str(),0,prefs.GetInt("DatabaseCache",4096));
    Library::Base::CreateDatabase(this->db);


    this->httpServer.Startup(database);

    // Start the indexer
    this->indexer.database    = database;
    this->indexer.Startup(directory);


    while(!this->Exited()){

        // tcp/ip port
//        int port( prefs.GetInt("ipPort",10543) );

        boost::system::error_code error;

        // Connect the next socket connection
        this->SetNextConnection();

        // loop, waiting for incomming connections
        this->ioService.run(error);

    }
}

void Server::SetNextConnection(){
    this->nextConnection.reset( new musik::core::server::Connection(this->ioService,this) );
    this->acceptor.async_accept(this->nextConnection->Socket(),boost::bind(&Server::AcceptConnection,this,boost::asio::placeholders::error));
}

void Server::AcceptConnection(const boost::system::error_code& error){
    if(!error){
        this->CleanupConnections();

        // Start the connection
        this->nextConnection->Startup();

        this->connections.push_back(this->nextConnection);
        this->SetNextConnection();
    }
}

void Server::CleanupConnections(){
    for(server::ConnectionVector::iterator connection=this->connections.begin();connection!=this->connections.end();){
        if( (*connection)->Exited() ){
            connection  = this->connections.erase(connection);
        }else{
            ++connection;
        }
    }
}

bool Server::CreateUser(const utfstring username,const utfstring plainTextPassword,const utfstring name){

    utfstring password( 
        UTF8_TO_UTF( 
            musik::core::Crypt::Encrypt(
                UTF_TO_UTF8(plainTextPassword),musik::core::Crypt::StaticSalt())));

    db::Statement stmt("INSERT INTO users (login,password,name) VALUES (?,?,?)",this->db);
    stmt.BindTextUTF(0,username.c_str());
    stmt.BindTextUTF(1,password.c_str());
    stmt.BindTextUTF(2,name.c_str());

    bool returnBool(stmt.Step()==db::Done);

    this->UsersUpdated();
    return returnBool;
}

bool Server::DeleteUser(const utfstring username){
    db::Statement stmt("DELETE FROM users WHERE login=?",this->db);
    stmt.BindTextUTF(0,username.c_str());

    bool returnBool(stmt.Step()==db::Done);

    this->UsersUpdated();
    return returnBool;
}

server::UserVector Server::AllUsers(){
    server::UserVector users;
    db::Statement stmt("SELECT id,name,login,password FROM users ORDER BY login",this->db);
    while(stmt.Step()==db::Row){
        users.push_back(server::UserPtr(new server::User(
            stmt.ColumnInt(0),
            stmt.ColumnTextUTF(2),
            stmt.ColumnTextUTF(3),
            stmt.ColumnTextUTF(1)
            )));
    }
    return users;
}

server::UserSessionVector Server::ConnectedUserSessions(){
    boost::mutex::scoped_lock lock(this->serverMutex);
    server::UserSessionVector userSessionVector;

    for(server::UserSessionMap::iterator userSession=this->connectedUsers.begin();userSession!=this->connectedUsers.end();++userSession){
        userSessionVector.push_back(userSession->second);
    }

    return userSessionVector;
}

bool Server::AddUserSession(server::UserSessionPtr userSession){
    {
        boost::mutex::scoped_lock lock(this->serverMutex);
        this->connectedUsers[userSession->UniqueId()] = userSession;
    }
    this->UserSessionsUpdated();

    return true;
}

bool Server::RemoveUserSession(server::UserSessionPtr userSession){
    {
        boost::mutex::scoped_lock lock(this->serverMutex);
        this->connectedUsers.erase(userSession->UniqueId());
    }
    this->UserSessionsUpdated();
    return true;
}

server::UserPtr Server::GetUser(const utfstring username){
    server::UserPtr user;
    db::Statement stmt("SELECT id,name,login,password FROM users WHERE login=?",this->db);
    stmt.BindTextUTF(0,username);
    if(stmt.Step()==db::Row){
        user.reset( new server::User(
            stmt.ColumnInt(0),
            stmt.ColumnTextUTF(2),
            stmt.ColumnTextUTF(3),
            stmt.ColumnTextUTF(1)
            ));
    }
    return user;
}


