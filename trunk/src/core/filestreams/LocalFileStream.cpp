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
#ifdef WIN32
#include "pch.hpp"
#else
#include <core/pch.hpp>
#endif

#include <core/filestreams/LocalFileStream.h>
#include <core/config.h>
#include <core/Common.h>
#include <core/config_filesystem.h>

//////////////////////////////////////////////////////////////////////////////
#ifdef UTF_WIDECHAR
#define UTFFopen    _wfopen
typedef fpos_t  stdioPositionType;
#else
#define UTFFopen    fopen
typedef fpos_t stdioPositionType;
#endif
//////////////////////////////////////////////////////////////////////////////


using namespace musik::core::filestreams;

//////////////////////////////////////////////////////////////////////////////
LocalFileStream::LocalFileStream()
 :file(NULL)
 ,filesize(-1)
{

}
LocalFileStream::~LocalFileStream(){
    this->Close();
}

bool LocalFileStream::Open(const utfchar *filename,unsigned int options){
#ifdef _DEBUG
	std::cerr << "LocalFileStream::Open()" << std::endl;
#endif
    if(filename==NULL){
        return false;
    }

    try{
        boost::filesystem::utfpath file(UTF(filename));
//#ifdef _DEBUG
//	std::cerr << "file: " << file  << std::endl;
//#endif
	if (!boost::filesystem::exists(file)) {
		std::cerr << "File not found" << std::endl;
	}
	if (!boost::filesystem::is_regular(file)) {
		std::cerr << "File not a regular file" << std::endl;
	}
        this->filesize  = (long)boost::filesystem::file_size(file);
//#ifdef _DEBUG
//	std::cerr << "filesize: " << this->filesize  << std::endl;
//#endif
        this->extension = file.extension();
//#ifdef _DEBUG
//	std::cerr << "extension: " << this->extension << std::endl;
//#endif
	this->file = UTFFopen(filename,UTF("rb"));
//#ifdef _DEBUG
//	std::cerr << "this->file: " << this->file  << std::endl;
//#endif
        this->fd  = new boost::iostreams::file_descriptor(file);
//#ifdef _DEBUG
//	std::cerr << "fd: " << this->fd  << std::endl;
//#endif
	this->fileStream = new boost::iostreams::stream<boost::iostreams::file_descriptor>(*this->fd);
//#ifdef _DEBUG
//	std::cerr << "this->fileStream: " << this->fileStream << std::endl;
//#endif
	this->fileStream->exceptions ( std::ios_base::eofbit | std::ios_base::failbit | std::ios_base::badbit );
        return this->file!=NULL;

    }
    catch(...){
        return false;
    }
}

bool LocalFileStream::Close(){
    if(this->file){
        if(fclose(this->file)==0){
            this->file  = NULL;
	    delete this->fd;
	    delete this->fileStream;
            return true;
        }
    }
    return false;
}

void LocalFileStream::Destroy(){
    delete this;
}

PositionType LocalFileStream::Read(void* buffer,PositionType readBytes){
    //return (PositionType)fread(buffer,1,readBytes,this->file);
    try	{
    	this->fileStream->read((char*)buffer, readBytes);
    }
    catch (std::ios_base::failure)	{
	if(this->fileStream->eof())	{
		//EOF reached
		return sizeof(buffer);
	}
	else	{
		std::cerr << "Error reading from file" << std::endl;
	}
	return 0;
    }
    return readBytes;
}

bool LocalFileStream::SetPosition(PositionType position){
    /*stdioPositionType newPosition  = (stdioPositionType)position;
    return fsetpos(this->file,&newPosition)==0;*/
    try	{
    	this->fileStream->seekp(position);
    	this->fileStream->seekg(position);
    }
    catch (std::ios_base::failure)	{
    	return false;
    }
    return true;
}

PositionType LocalFileStream::Position(){
    /*stdioPositionType currentPosition(0);
    if(fgetpos(this->file,&currentPosition)==0){
        return (PositionType)currentPosition;
    }
    return -1;*/
    return this->fileStream->tellg();
}

bool LocalFileStream::Eof(){
    //return feof(this->file)!=0;
    return this->fileStream->eof();
}

long LocalFileStream::Filesize(){
    return this->filesize;
}

const utfchar* LocalFileStream::Type(){
    return this->extension.c_str();
}

