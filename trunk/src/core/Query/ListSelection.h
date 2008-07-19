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

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <set>
#include <map>
#include <vector>
#include <string>

#include <core/config.h>
#include <core/Query/ListBase.h>

//////////////////////////////////////////////////////////////
// Forward declarations
//////////////////////////////////////////////////////////////
namespace musik{ namespace core{
    namespace Library{
        class Base;
    }
} }


namespace musik{ namespace core{
    namespace Query{

        //////////////////////////////////////////
        ///\brief
        ///ListSelection is the query used when listing tracks and metalists from a metalist (genres, artists, etc) selection.
        ///
        ///Write detailed description for ListSelection here.
        ///
        ///\see
        ///Query::ListBase
        //////////////////////////////////////////
        class ListSelection : public Query::ListBase{
            public:
                ListSelection(void);
                ~ListSelection(void);

                void SelectMetadata(const char* metakey,DBINT metadataId);
                void RemoveMetadata(const char* metakey,DBINT metadataId);
                void ClearMetadata(const char* metakey=NULL);

                void SelectionOrderSensitive(bool sensitive);
            protected:
                friend class Library::Base;
                friend class Library::LocalDB;
                friend class server::Connection;

                virtual std::string Name();
                virtual bool ParseQuery(Library::Base *library,db::Connection &db);

                Ptr copy() const;

                virtual bool RecieveQuery(musik::core::xml::ParserNode &queryNode);

            private:
                typedef std::map<std::string,std::set<DBINT>> SelectedMetadata;


                //////////////////////////////////////////
                ///\brief
                ///A map of selected metakeys
                //////////////////////////////////////////
                SelectedMetadata selectedMetadata;

                //////////////////////////////////////////
                ///\brief
                ///Setting if selection is sensitive to order of selection
                //////////////////////////////////////////
                bool selectionOrderSensitive;

                //////////////////////////////////////////
                ///\brief
                ///A list of the metakeys selection order
                //////////////////////////////////////////
                std::vector<std::string> metakeySelectionOrder;


                inline void SQLPrependWhereOrAnd(std::string &sql);
                void SQLSelectQuery(const char *metakey,const char *sqlStart,const char *sqlEnd,std::set<std::string> &metakeysSelected,std::string &sqlSelectTrackWhere,Library::Base *library);
                void QueryForMetadata(const char *metakey,const char *sql,std::set<std::string> &metakeysQueried,Library::Base *library,db::Connection &db);

        };

    }
} }


