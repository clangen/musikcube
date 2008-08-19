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
#include <core/Query/Factory.h>

#include <core/xml/Parser.h>
#include <core/xml/ParserNode.h>
#include <core/xml/Writer.h>
#include <core/xml/WriterNode.h>

#include <boost/bind.hpp>

using namespace musik::core::server;


Connection::Connection(boost::asio::io_service &ioService)
 :socket(ioService)
 ,Base(UTF("Server"))
{
}

Connection::~Connection(void){
    this->Exit();
    this->threads.join_all();
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

    musik::core::xml::Parser xmlParser(&this->socket);

    try{

        // Test waiting for a Node
        if( musik::core::xml::ParserNode root = xmlParser.ChildNode("musik") ){
            // musik node initialized

            musik::core::Query::QueryMap queryMap;
            musik::core::Query::Factory::GetQueries(queryMap);

            // Loop waiting for queries
            while( musik::core::xml::ParserNode queryNode = root.ChildNode("query") ){
                // Got a query
                std::string queryType(queryNode.Attributes()["type"]);
                
                musik::core::Query::QueryMap::iterator queryIt = queryMap.find(queryType);
                if(queryIt!=queryMap.end()){
                    // Query type exists, lets create a copy
                    musik::core::Query::Ptr query( queryIt->second->copy() );
                    try{
                        query->queryId  = boost::lexical_cast<unsigned int>(queryNode.Attributes()["id"]);
                        query->uniqueId = boost::lexical_cast<unsigned int>(queryNode.Attributes()["uid"]);
                    }catch(...){}

                    if(query->RecieveQuery(queryNode)){

                        unsigned int options(0);
                        try{
                            options = boost::lexical_cast<unsigned int>(queryNode.Attributes()["options"]);
                        }catch(...){}

                        // TODO: check for AddQuery options in tag
                        this->AddQuery( *query,options|musik::core::Query::CopyUniqueId );

                    }

                }
            }
        }
        
    }
    catch(...){
        // Connection dropped
        std::cout << "Connection dropped" << std::endl;
    }

    this->Exit();

}

void Connection::ParseThread(){

    Preferences prefs("Server");

    utfstring database(this->GetDBPath());
    this->db.Open(database.c_str(),0,prefs.GetInt("DatabaseCache",4096));

    while(!this->Exited()){
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

    this->db.Close();

}

void Connection::WriteThread(){

    musik::core::xml::Writer xmlWriter(&this->socket);

    try{
        // Lets start with a <musik> node
        musik::core::xml::WriterNode musikNode(xmlWriter,"musik");

        while(!this->Exited()){

            Query::Ptr sendQuery;
            // Wait for outgoingQueries
            {
                boost::mutex::scoped_lock lock(this->libraryMutex);
                if(this->outgoingQueries.empty()){
                    this->waitCondition.wait(lock);
                }

                if(!this->outgoingQueries.empty()){
                    sendQuery   = this->outgoingQueries.front();
                }
            }

            // now there should be sendQuery
            if(sendQuery){
                // Send the query
                {
                    musik::core::xml::WriterNode queryNode(musikNode,"queryresults");
                    queryNode.Attributes()["type"]  = sendQuery->Name();
                    queryNode.Attributes()["id"]    = boost::lexical_cast<std::string>(sendQuery->queryId);
                    queryNode.Attributes()["uid"]   = boost::lexical_cast<std::string>(sendQuery->uniqueId);

                    sendQuery->SendResults(queryNode,this);
                }

                // Remove the query from the queue
                {
                    boost::mutex::scoped_lock lock(this->libraryMutex);
                    // We can not be sure that the query is the front query
                    //so lets loop through the outgoing queries and remove the match
                    for(QueryList::iterator query=this->outgoingQueries.begin();query!=this->outgoingQueries.end();){
                        if( sendQuery== *query ){
                            query   = this->outgoingQueries.erase(query);
                        }else{
                            ++query;
                        }
                    }
                }
            }
        }
    }
    catch(...){
    }
    this->Exit();
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

void Connection::Exit(){
    {
        boost::mutex::scoped_lock lock(this->libraryMutex);
        if(!this->exit){
            if(this->socket.is_open()){
                this->socket.close();
            }
        }
        this->exit    = true;
    }
    this->waitCondition.notify_all();
}
