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
#include <core/server/Connection.h>
#include <core/Preferences.h>
#include <core/Query/Base.h>

using namespace musik::core::server;


Connection::Connection(boost::asio::io_service &ioService)
 :socket(ioService)
{
    this->identifier    = UTF("server");
}

Connection::~Connection(void){
    this->threads.join_all();
    this->socket.close();
}

boost::asio::ip::tcp::socket &Connection::Socket(){
    return this->socket;
}

//////////////////////////////////////////
///\brief
///Start the Connections threads
///
///3 different threads will be started:
///1. Reading from the socket and parsing the XML, adding queries to the queue.
///2. One thread for executing the Queries
///3. And one thread for sending the results
//////////////////////////////////////////
bool Connection::Startup(){

    std::cout << "Connection::Startup" << std::endl;

    this->threads.create_thread(boost::bind(&Connection::ReadThread,this));
    this->threads.create_thread(boost::bind(&Connection::ParseThread,this));
    this->threads.create_thread(boost::bind(&Connection::WriteThread,this));
    
    return true;
}

void Connection::ReadThread(){
}

void Connection::ParseThread(){

    Preferences prefs("Server");

    utfstring database(this->GetDBPath());
    this->db.Open(database.c_str(),0,prefs.GetInt("DatabaseCache",4096));

    while(!this->Exit()){
        Query::Ptr query(this->GetNextQuery());

        if(query){    // No empty query

            ////////////////////////////////////////////////////////////
            // Add to the finished queries
            {
                boost::mutex::scoped_lock lock(this->libraryMutex);
                this->bCurrentQueryCanceled    = false;
                this->runningQuery    = query;
                this->outgoingQueries.push_back(query);

                // Set query as started
                query->status |= Query::Base::Status::Started;
            }

            ////////////////////////////////////////////////////////////
            // Lets parse the query
            query->ParseQuery(this,this->db);
            {
                boost::mutex::scoped_lock lock(this->libraryMutex);
                this->runningQuery.reset();
                // And set it as finished
                query->status |= Query::Base::Status::Ended;
            }

            ////////////////////////////////////////////////////////////
            // Notify that the Query is finished.
            this->waitCondition.notify_all();

        }else{

            ////////////////////////////////////////////////////////////
            // Tricky part, waiting for queries to be added.
            // Not sure I'm doing this the right way.
            // Could this part lead to a deadlock???
            boost::mutex::scoped_lock lock(this->libraryMutex);
            if(!this->exit && this->incomingQueries.size()==0 ){
                this->waitCondition.wait(lock);
            }
        }
    }
}

void Connection::WriteThread(){
}

//////////////////////////////////////////
///\brief
///Cancel the current running query
///
///This method will also send a sqlite3_interrupt to cancel the
///current running SQL Query
//////////////////////////////////////////
void Connection::CancelCurrentQuery( ){
    this->bCurrentQueryCanceled    = true;
    this->db.Interrupt();
}

utfstring Connection::GetInfo(){
    return UTF("");
}

