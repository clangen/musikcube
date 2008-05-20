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
#include <core/Thumbnail.h>
#include <core/Common.h>


//////////////////////////////////////////////////////////////////////////////

using namespace musik::core;

//////////////////////////////////////////////////////////////////////////////

Thumbnail::Thumbnail()
: trackId(0)
, maxWidth(0)
, maxHeight(0)
, image(NULL)
{
    this->infoQuery.ThumbnailInfoFetched.connect(this,&Thumbnail::OnThumbnailInfo);
}


Thumbnail::~Thumbnail(){
    this->ClearImage();
}


void Thumbnail::Load(int trackId,musik::core::LibraryPtr library){
    if(library){
        this->infoQuery.GetThumbnail(trackId);
        this->trackId   = trackId;

        library->AddQuery(this->infoQuery,musik::core::Query::CancelSimilar);
    }
}

void Thumbnail::OnThumbnailInfo(utfstring filename){
    if(!filename.empty()){
        // Lets load the file using boost::gil
        // Convert to UTF8
        std::string file    = musik::core::ConvertUTF8(filename);

        try{
            this->ClearImage();
            this->image = new boost::gil::rgb8_image_t();
            boost::gil::jpeg_read_image(file,*this->image);

            this->ImageLoaded(true);
            return;
        }
        catch(...){
            // supress errors
        }
    }
    this->ImageLoaded(false);
}

Thumbnail::ImagePtr Thumbnail::GetImage(int width,int height){
    if(this->image){
        ImagePtr image(new boost::gil::rgba8_image_t(width,height));
        boost::gil::resize_view(boost::gil::const_view(*this->image), view(*image.get()), boost::gil::bilinear_sampler());
        return image;
    }
    return ImagePtr();
}


void Thumbnail::ClearImage(){
    if(this->image!=NULL){
        delete this->image;
        this->image=NULL;
    }
}

