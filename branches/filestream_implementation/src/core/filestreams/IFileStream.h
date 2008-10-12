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

#include <core/config.h>

//////////////////////////////////////////////////////////////////////////////
namespace musik{ namespace core{ namespace filestreams{
//////////////////////////////////////////////////////////////////////////////

typedef long PositionType;

class IFileStream{
    public:


        //////////////////////////////////////////
        ///\brief
        ///Open the file for reading
        ///
        ///\param filename
        ///Filename to open
        ///
        ///\returns
        ///true if file is opened succesfully
        //////////////////////////////////////////
        virtual bool Open(const utfchar *filename,unsigned int options=0)=0;

        //////////////////////////////////////////
        ///\brief
        ///Close the file
        ///
        ///\returns
        ///true if file is closed succesfully
        //////////////////////////////////////////
        virtual bool Close()=0;

        //////////////////////////////////////////
        ///\brief
        ///Destroy the object (not the file)
        //////////////////////////////////////////
        virtual void Destroy()=0;

        //////////////////////////////////////////
        ///\brief
        ///Read some from the file
        ///
        ///\param buffer
        ///Buffer to write the read content to
        ///
        ///\param readBytes
        ///How much (max) to read from the stream
        ///
        ///\returns
        ///how much has acctually been read
        //////////////////////////////////////////
        virtual PositionType Read(void* buffer,PositionType readBytes)=0;

        //////////////////////////////////////////
        ///\brief
        ///Set the position in the file
        ///
        ///\param position
        ///what position to set
        ///
        ///\returns
        ///true if the seek is succesful
        //////////////////////////////////////////
        virtual bool SetPosition(PositionType position)=0;

        //////////////////////////////////////////
        ///\brief
        ///Get the position in the file
        ///
        ///\returns
        ///Current position
        //////////////////////////////////////////
        virtual PositionType Position()=0;

        //////////////////////////////////////////
        ///\brief
        ///If the position in the "end of file"
        ///
        ///\returns
        ///true if file is ended
        //////////////////////////////////////////
        virtual bool Eof()=0;


        //////////////////////////////////////////
        ///\brief
        ///Get files size
        ///
        ///\returns
        ///-1 if unknown
        //////////////////////////////////////////
        virtual long Filesize()=0;

};

//////////////////////////////////////////////////////////////////////////////
} } }
//////////////////////////////////////////////////////////////////////////////


